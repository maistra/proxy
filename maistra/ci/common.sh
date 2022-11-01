#!/bin/bash

export CC=clang CXX=clang++

ARCH=$(uname -p)
if [ "${ARCH}" = "ppc64le" ]; then
  ARCH="ppc"
fi
export ARCH

OUTPUT_TO_IGNORE="\
INFO: From|\
Fixing bazel-out|\
cache:INFO:  - ok|\
processwrapper-sandbox.*execroot.*io_istio_proxy|\
proto is unused\
"

COMMON_FLAGS="\
    --config=release \
    --config=${ARCH} \
"

if [ -n "${BAZEL_REMOTE_CACHE}" ]; then
  COMMON_FLAGS+=" --remote_cache=${BAZEL_REMOTE_CACHE} "
elif [ -n "${BAZEL_DISK_CACHE}" ]; then
  COMMON_FLAGS+=" --disk_cache=${BAZEL_DISK_CACHE} "
fi

if [ -n "${CI}" ]; then
  COMMON_FLAGS+=" --config=ci-config " 
fi

function bazel_build() {
  bazel build \
    ${COMMON_FLAGS} \
    "${@}" \
  2>&1 | grep -v -E "${OUTPUT_TO_IGNORE}"
}

function bazel_test() {
  bazel test \
    ${COMMON_FLAGS} \
    --build_tests_only \
    "${@}" \
  2>&1 | grep -v -E "${OUTPUT_TO_IGNORE}"
}

# Fix path to the vendor deps
#sed -i "s|=/work/|=$(pwd)/|" maistra/bazelrc-vendor
