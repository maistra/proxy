#!/bin/bash

ARCH=$(uname -p)
if [ "${ARCH}" = "ppc64le" ]; then
  ARCH="ppc"
fi
export ARCH

COMMON_FLAGS="\
    --incompatible_linkopts_to_linklibs \
    --config=release \
    --config=${ARCH} \
    --local_ram_resources=12288 \
    --local_cpu_resources=6 \
    --jobs=3 \
    --@envoy//bazel:http3=false \
    --deleted_packages=@envoy//test/common/quic,@envoy//test/common/quic/platform \
    --verbose_failures \
    --color=no \
"

if [ -n "${BAZEL_REMOTE_CACHE}" ]; then
  COMMON_FLAGS+=" --remote_cache=${BAZEL_REMOTE_CACHE} "
elif [ -n "${BAZEL_DISK_CACHE}" ]; then
  COMMON_FLAGS+=" --disk_cache=${BAZEL_DISK_CACHE} "
fi

function bazel_build() {
  bazel build \
    ${COMMON_FLAGS} \
    "${@}"
}

function bazel_test() {
  bazel test \
    ${COMMON_FLAGS} \
    --build_tests_only \
    "${@}"
}

# Fix path to the vendor deps
sed -i "s|=/work/|=$(pwd)/|" maistra/bazelrc-vendor
sed -i "s|/work/|$(pwd)/|" maistra/vendor/proxy_wasm_cpp_sdk/toolchain/cc_toolchain_config.bzl
