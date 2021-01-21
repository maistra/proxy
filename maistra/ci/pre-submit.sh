#!/bin/bash

set -e
set -o pipefail
set -x

source /opt/rh/gcc-toolset-9/enable

DIR=$(cd $(dirname $0) ; pwd -P)
source "${DIR}/common.sh"

ARCH=$(uname -p)
if [ "${ARCH}" = "ppc64le" ]; then
  ARCH="ppc"
fi
export ARCH

export BUILD_SCM_REVISION="Maistra PR #${PULL_NUMBER:-undefined}"
export BUILD_SCM_STATUS="SHA=${PULL_PULL_SHA:-undefined}"

# Fix path to the vendor deps
sed -i "s|=/work/|=$(pwd)/|" maistra/bazelrc-vendor

# Build
bazel build \
  --incompatible_linkopts_to_linklibs \
  --config=release \
  --config=${ARCH} \
  --local_ram_resources=12288 \
  --local_cpu_resources=4 \
  --jobs=4 \
  --disk_cache=/bazel-cache \
  //src/envoy:envoy \
  2>&1 | grep -v -E "${OUTPUT_TO_IGNORE}"

echo "Build succeeded. Binary generated:"
bazel-bin/src/envoy/envoy --version

# Run tests
bazel test \
  --incompatible_linkopts_to_linklibs \
  --config=release \
  --config=${ARCH} \
  --local_ram_resources=12288 \
  --local_cpu_resources=4 \
  --jobs=4 \
  --test_output=all \
  --build_tests_only \
  --test_env=ENVOY_IP_TEST_VERSIONS=v4only \
  --disk_cache=/bazel-cache \
  //src/... \
  2>&1 | grep -v -E "${OUTPUT_TO_IGNORE}"
