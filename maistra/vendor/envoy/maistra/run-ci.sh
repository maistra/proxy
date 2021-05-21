#!/bin/bash

set -e
set -o pipefail
set -x

source /opt/rh/gcc-toolset-9/enable

ARCH=$(uname -p)
if [ "${ARCH}" = "ppc64le" ]; then
  ARCH="ppc"
fi
export ARCH

export BUILD_SCM_REVISION="Maistra PR #${PULL_NUMBER:-undefined}"
export BUILD_SCM_STATUS="SHA=${PULL_PULL_SHA:-undefined}"

# Build
time bazel build \
  --incompatible_linkopts_to_linklibs \
  --local_ram_resources=12288 \
  --local_cpu_resources=4 \
  --jobs=4 \
  --disk_cache=/bazel-cache \
  //source/exe:envoy-static

echo "Build succeeded. Binary generated:"
bazel-bin/source/exe/envoy-static --version

# Run tests
time bazel test \
  --incompatible_linkopts_to_linklibs \
  --local_ram_resources=12288 \
  --local_cpu_resources=4 \
  --jobs=4 \
  --build_tests_only \
  --test_env=ENVOY_IP_TEST_VERSIONS=v4only \
  --test_output=all \
  --disk_cache=/bazel-cache \
  //test/...

