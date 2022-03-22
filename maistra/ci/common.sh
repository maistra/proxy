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
    --disk_cache=/bazel-cache \
    --@envoy//bazel:http3=false \
    --deleted_packages=@envoy//test/common/quic,@envoy//test/common/quic/platform \
    --verbose_failures \
    --color=no \
"

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
