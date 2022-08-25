// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/abseil_string_number_conversions.h"

#include "base/strings/string_number_conversions_internal.h"
#include "base/strings/string_piece.h"
#include "absl/numeric/int128.h"

namespace gurl_base {

bool HexStringToUInt128(StringPiece input, absl::uint128* output) {
  return internal::HexStringToIntImpl(input, *output);
}

}  // namespace base
