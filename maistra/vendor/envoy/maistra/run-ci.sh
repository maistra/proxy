#!/bin/bash

set -e
set -o pipefail
set -x

DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
source "$DIR/common.sh"

export BUILD_SCM_REVISION="Maistra PR #${PULL_NUMBER:-undefined}"
export BUILD_SCM_STATUS="SHA=${PULL_PULL_SHA:-undefined}"

# Build
time bazel build \
  ${COMMON_FLAGS} \
  //source/exe:envoy-static

echo "Build succeeded. Binary generated:"
bazel-bin/source/exe/envoy-static --version

# By default, `bazel test` command performs simultaneous
# build and test activity.
# The following build step helps reduce resources usage
# by compiling tests first.
# Build tests
time bazel build \
  ${COMMON_FLAGS} \
  --build_tests_only \
  //test/...

# Run tests
time bazel test \
  ${COMMON_FLAGS} \
  --build_tests_only \
  --flaky_test_attempts=5 \
  -- \
  //test/... \
  -//test/server:guarddog_impl_test

time bazel test \
  ${COMMON_FLAGS} \
  --build_tests_only \
  --flaky_test_attempts=5 \
  //test/server:guarddog_impl_test

