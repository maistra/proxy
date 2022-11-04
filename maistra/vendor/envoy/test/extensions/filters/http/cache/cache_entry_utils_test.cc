#include "source/extensions/filters/http/cache/cache_entry_utils.h"

#include "test/test_common/utility.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Cache {
namespace {

TEST(Coverage, CacheEntryStatusString) {
  EXPECT_EQ(cacheEntryStatusString(CacheEntryStatus::Ok), "Ok");
  EXPECT_EQ(cacheEntryStatusString(CacheEntryStatus::Unusable), "Unusable");
  EXPECT_EQ(cacheEntryStatusString(CacheEntryStatus::RequiresValidation), "RequiresValidation");
  EXPECT_EQ(cacheEntryStatusString(CacheEntryStatus::FoundNotModified), "FoundNotModified");
  EXPECT_EQ(cacheEntryStatusString(CacheEntryStatus::LookupError), "LookupError");
  EXPECT_ENVOY_BUG(cacheEntryStatusString(static_cast<CacheEntryStatus>(99)),
                   "Unexpected CacheEntryStatus");
}

TEST(Coverage, CacheEntryStatusStream) {
  std::ostringstream stream;
  stream << CacheEntryStatus::Ok;
  EXPECT_EQ(stream.str(), "Ok");
}

} // namespace
} // namespace Cache
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
