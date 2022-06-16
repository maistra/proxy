#!/bin/bash

set -e
set -u
set -o pipefail
set -x

DIR=$(cd $(dirname $0) ; pwd -P)
source "${DIR}/common.sh"

export BUILD_SCM_REVISION="Maistra PR #${PULL_NUMBER:-undefined}"
export BUILD_SCM_STATUS="SHA=${PULL_PULL_SHA:-undefined}"

# Fix path to the vendor deps
sed -i "s|=/work/|=$(pwd)/|" maistra/bazelrc-vendor

# Build
bazel build \
  ${COMMON_FLAGS} \
  //src/envoy:envoy \
  2>&1 | grep -v -E "${OUTPUT_TO_IGNORE}"

echo "Build succeeded. Binary generated:"
bazel-bin/src/envoy/envoy --version

# Run tests
bazel test \
  ${COMMON_FLAGS} \
  --test_output=all \
  --build_tests_only \
  --test_env=ENVOY_IP_TEST_VERSIONS=v4only \
  //src/... \
  2>&1 | grep -v -E "${OUTPUT_TO_IGNORE}"
