#!/bin/bash

OUTPUT_TO_IGNORE="\
linux-sandbox-pid1.cc|\
process-tools.cc|\
linux-sandbox.cc|\
rules_foreign_cc: Cleaning temp directories|\
proto but not used|\
INFO: From\
"

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
    --local_cpu_resources=8 \
    --jobs=4 \
    --disk_cache=/bazel-cache \
"

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
    --test_env=ENVOY_IP_TEST_VERSIONS=v4only \
    "${@}" \
  2>&1 | grep -v -E "${OUTPUT_TO_IGNORE}"
}

# Fix path to the vendor deps
sed -i "s|=/work/|=$(pwd)/|" maistra/bazelrc-vendor
sed -i "s|/work/|$(pwd)/|" maistra/vendor/proxy_wasm_cpp_sdk/toolchain/cc_toolchain_config.bzl
