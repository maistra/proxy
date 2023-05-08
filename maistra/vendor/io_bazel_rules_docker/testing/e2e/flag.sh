#!/usr/bin/env bash
set -ex
# Copyright 2021 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
source ./testing/e2e/util.sh

# Tests that the flags are passed correctly.

# Must be invoked from the root of the repo.
ROOT=$PWD

function test_flag() {
  cd "${ROOT}"
  clear_docker
  EXPECT_CONTAINS "$(bazel run testdata:flag_image -- -arg='Hello World!')" "Hello World!"
}

# Call function above
test_flag
