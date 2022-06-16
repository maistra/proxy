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

COMMON_FLAGS="\
    --local_resources 12288,4.0,1.0 \
    --local_cpu_resources=6 \
    --jobs=4 \
    --color=no \
"

if [ -n "${BAZEL_REMOTE_CACHE}" ]; then
  COMMON_FLAGS+=" --remote_cache=${BAZEL_REMOTE_CACHE} "
elif [ -n "${BAZEL_DISK_CACHE}" ]; then
  COMMON_FLAGS+=" --disk_cache=${BAZEL_DISK_CACHE} "
fi

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
  --test_env=ENVOY_IP_TEST_VERSIONS=v4only \
  --test_output=all \
  //test/...
