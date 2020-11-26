#!/bin/bash

set -e
set -u
set -o pipefail
set -x

ARCH=$(uname -p)
if [ "${ARCH}" = "ppc64le" ]; then
  ARCH="ppc"
fi
export ARCH

export BUILD_SCM_REVISION="Maistra PR #${PULL_NUMBER:-undefined}"
export BUILD_SCM_STATUS="SHA=${PULL_PULL_SHA:-undefined}"

# Build
time bazel build \
  --local_resources 12288,4.0,1.0 \
  --jobs=4 \
  --disk_cache=/bazel-cache \
  //source/exe:envoy-static

echo "Build succeeded. Binary generated:"
bazel-bin/source/exe/envoy-static --version

# Run tests
time bazel test \
  --local_resources 12288,4.0,1.0 \
  --jobs=4 \
  --build_tests_only \
  --test_env=ENVOY_IP_TEST_VERSIONS=v4only \
  --test_output=all \
  --disk_cache=/bazel-cache \
  //test/...
