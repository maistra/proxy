#pragma once

#include "envoy/common/key_value_store.h"
#include "envoy/event/dispatcher.h"
#include "envoy/filesystem/filesystem.h"

#include "source/common/common/logger.h"

#include "absl/container/flat_hash_map.h"

namespace Envoy {

// This is the base implementation of the KeyValueStore. It handles the various
// functions other than flush(), which will be implemented by subclasses.
//
// Note this implementation HAS UNBOUNDED SIZE.
// It is assumed the callers manage the number of entries. Use with care.
class KeyValueStoreBase : public KeyValueStore,
                          public Logger::Loggable<Logger::Id::key_value_store> {
public:
  // Sets up flush() for the configured interval.
  KeyValueStoreBase(Event::Dispatcher& dispatcher, std::chrono::seconds flush_interval);

  // If |contents| is in the form of
  // [length]\n[key][length]\n[value]
  // parses key value pairs from |contents| into the store provided.
  // Returns true on success and false on failure.
  bool parseContents(absl::string_view contents,
                     absl::flat_hash_map<std::string, std::string>& store) const;
  std::string error;
  // KeyValueStore
  void addOrUpdate(absl::string_view key, absl::string_view value) override;
  void remove(absl::string_view key) override;
  absl::optional<absl::string_view> get(absl::string_view key) override;
  void iterate(ConstIterateCb cb) const override;

protected:
  const Event::TimerPtr flush_timer_;
  absl::flat_hash_map<std::string, std::string> store_;
};

} // namespace Envoy
