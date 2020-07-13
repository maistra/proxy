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

# Fix path to the vendor deps
sed -i "s|=/work/|=$(pwd)/|" maistra/bazelrc-vendor

# Build
bazel build \
  --config=release \
  --config=${ARCH} \
  --local_resources 12288,6.0,1.0 \
  --jobs=6 \
  //src/envoy:envoy

echo "Build succeeded. Binary generated:"
bazel-bin/src/envoy/envoy --version

# Run tests
bazel test \
  --config=release \
  --config=${ARCH} \
  --local_resources 12288,6.0,1.0 \
  --jobs=6 \
  //src/...
