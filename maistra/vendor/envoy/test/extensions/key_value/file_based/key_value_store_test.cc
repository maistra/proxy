#include <memory>
#include <string>

#include "source/common/common/key_value_store_base.h"
#include "source/extensions/key_value/file_based/config.h"

#include "test/mocks/event/mocks.h"
#include "test/test_common/environment.h"
#include "test/test_common/file_system_for_test.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::NiceMock;

namespace Envoy {
namespace Extensions {
namespace KeyValue {
namespace {

class KeyValueStoreTest : public testing::Test {
protected:
  KeyValueStoreTest() : filename_(TestEnvironment::temporaryPath("key_value_store")) {
    TestEnvironment::removePath(filename_);
    createStore();
  }

  void createStore() {
    store_ = std::make_unique<FileBasedKeyValueStore>(dispatcher_, std::chrono::seconds{5},
                                                      Filesystem::fileSystemForTest(), filename_);
  }
  NiceMock<Event::MockDispatcher> dispatcher_;
  std::string filename_;
  std::unique_ptr<FileBasedKeyValueStore> store_{};
};

TEST_F(KeyValueStoreTest, Basic) {
  EXPECT_EQ(absl::nullopt, store_->get("foo"));
  store_->addOrUpdate("foo", "bar");
  EXPECT_EQ("bar", store_->get("foo").value());
  store_->addOrUpdate("foo", "eep");
  EXPECT_EQ("eep", store_->get("foo").value());
  store_->remove("foo");
  EXPECT_EQ(absl::nullopt, store_->get("foo"));
}

TEST_F(KeyValueStoreTest, Persist) {
  store_->addOrUpdate("foo", "bar");
  store_->addOrUpdate("ba\nz", "ee\np");
  store_->flush();

  createStore();

  KeyValueStore::ConstIterateCb validate = [](const std::string& key, const std::string&) {
    EXPECT_TRUE(key == "foo" || key == "ba\nz");
    return KeyValueStore::Iterate::Continue;
  };

  EXPECT_EQ("bar", store_->get("foo").value());
  EXPECT_EQ("ee\np", store_->get("ba\nz").value());
  store_->iterate(validate);
}

TEST_F(KeyValueStoreTest, Iterate) {
  store_->addOrUpdate("foo", "bar");
  store_->addOrUpdate("baz", "eep");

  int full_counter = 0;
  KeyValueStore::ConstIterateCb validate = [&full_counter](const std::string& key,
                                                           const std::string&) {
    ++full_counter;
    EXPECT_TRUE(key == "foo" || key == "baz");
    return KeyValueStore::Iterate::Continue;
  };
  store_->iterate(validate);
  EXPECT_EQ(2, full_counter);

  int stop_early_counter = 0;
  KeyValueStore::ConstIterateCb stop_early = [&stop_early_counter](const std::string&,
                                                                   const std::string&) {
    ++stop_early_counter;
    return KeyValueStore::Iterate::Break;
  };
  store_->iterate(stop_early);
  EXPECT_EQ(1, stop_early_counter);
}

TEST_F(KeyValueStoreTest, HandleBadFile) {
  auto checkBadFile = [this](std::string file, std::string error) {
    TestEnvironment::writeStringToFileForTest(filename_, file, true);
    EXPECT_LOG_CONTAINS("warn", error, createStore());

    // File will be parsed up until error.
    EXPECT_EQ("bar", store_->get("foo").value());
  };
  checkBadFile("3\nfoo3\nbar3", "Bad file: no newline");
  checkBadFile("3\nfoo3\nbar\n", "Bad file: no length");
  checkBadFile("3\nfoo3\nbarasd\n", "Bad file: no length");
  checkBadFile("3\nfoo3\nbar3\na", "Bad file: insufficient contents");
}

#ifndef WIN32
TEST_F(KeyValueStoreTest, HandleInvalidFile) {
  filename_ = "/path/that/does/not/exist/foo";
  createStore();
  EXPECT_LOG_CONTAINS("error", "Failed to flush cache to file /path/that/does/not/exist/foo", store_->flush());
}
#endif

} // namespace
} // namespace KeyValue
} // namespace Extensions
} // namespace Envoy
