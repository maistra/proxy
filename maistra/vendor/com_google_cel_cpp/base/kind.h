// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_BASE_KIND_H_
#define THIRD_PARTY_CEL_CPP_BASE_KIND_H_

#include <cstdint>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"

namespace cel {

enum class Kind : uint8_t {
  kNullType = 0,
  kError,
  kDyn,
  kAny,
  kType,
  kBool,
  kInt,
  kUint,
  kDouble,
  kString,
  kBytes,
  kEnum,
  kDuration,
  kTimestamp,
  kList,
  kMap,
  kStruct,

  // INTERNAL: Do not exceed 127. Implementation details rely on the fact that
  // we can store `Kind` using 7 bits.
};

ABSL_ATTRIBUTE_PURE_FUNCTION absl::string_view KindToString(Kind kind);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_KIND_H_
