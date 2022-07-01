#!/bin/bash

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

# Run tests
time bazel test \
  ${COMMON_FLAGS} \
  --build_tests_only \
  //test/...

