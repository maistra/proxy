#!/bin/bash

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
    --config=clang \
    --local_ram_resources=${LOCAL_RAM_RESOURCES:-12288} \
    --local_cpu_resources=${LOCAL_CPU_RESOURCES:-6} \
    --jobs=${LOCAL_JOBS:-3} \
    --@envoy//bazel:http3=false \
    --deleted_packages=@envoy//test/common/quic,@envoy//test/common/quic/platform \
    --verbose_failures \
    --color=no \
    --show_progress_rate_limit=10 \
"

if [ -n "${BAZEL_REMOTE_CACHE}" ]; then
  COMMON_FLAGS+=" --remote_cache=${BAZEL_REMOTE_CACHE} "
elif [ -n "${BAZEL_DISK_CACHE}" ]; then
  COMMON_FLAGS+=" --disk_cache=${BAZEL_DISK_CACHE} "
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
sed -i "s|=/work/|=$(pwd)/|" maistra/bazelrc-vendor
sed -i "s|/work/|$(pwd)/|" maistra/vendor/proxy_wasm_cpp_sdk/toolchain/cc_toolchain_config.bzl
