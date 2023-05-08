// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_HEAP_HANDLE_H_
#define INCLUDE_CPPGC_HEAP_HANDLE_H_

#include "v8config.h"  // NOLINT(build/include_directory)

namespace cppgc {

namespace internal {
class HeapBase;
class WriteBarrierTypeForCagedHeapPolicy;
}  // namespace internal

/**
 * Opaque handle used for additional heap APIs.
 */
class HeapHandle {
 private:
  HeapHandle() = default;

  V8_INLINE bool is_incremental_marking_in_progress() const {
    return is_incremental_marking_in_progress_;
  }

  V8_INLINE bool is_young_generation_enabled() const {
    return is_young_generation_enabled_;
  }

  bool is_incremental_marking_in_progress_ = false;
  bool is_young_generation_enabled_ = false;

  friend class internal::HeapBase;
  friend class internal::WriteBarrierTypeForCagedHeapPolicy;
};

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_HEAP_HANDLE_H_
