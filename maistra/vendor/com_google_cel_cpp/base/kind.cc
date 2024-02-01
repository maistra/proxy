// Copyright 2021 Google LLC
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

#include "base/kind.h"

namespace cel {

absl::string_view KindToString(Kind kind) {
  switch (kind) {
    case Kind::kNullType:
      return "null_type";
    case Kind::kDyn:
      return "dyn";
    case Kind::kAny:
      return "any";
    case Kind::kType:
      return "type";
    case Kind::kBool:
      return "bool";
    case Kind::kInt:
      return "int";
    case Kind::kUint:
      return "uint";
    case Kind::kDouble:
      return "double";
    case Kind::kString:
      return "string";
    case Kind::kBytes:
      return "bytes";
    case Kind::kEnum:
      return "enum";
    case Kind::kDuration:
      return "duration";
    case Kind::kTimestamp:
      return "timestamp";
    case Kind::kList:
      return "list";
    case Kind::kMap:
      return "map";
    case Kind::kStruct:
      return "struct";
    default:
      return "*error*";
  }
}

}  // namespace cel
