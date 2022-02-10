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
    --local_cpu_resources=6 \
    --jobs=3 \
    --disk_cache=/bazel-cache \
"

function fix_include_paths() {
  pushd maistra/vendor/envoy
  patch -p1 < ./maistra/patches/0001-modify-envoy-paths.patch
  ./maistra/patches/modify-include-paths.sh
  popd

  patch -p1 < ./maistra/ci/modify-proxy-paths.patch
  find ./src ./extensions ./test -name BUILD -print0 | xargs -0 sed -i 's/"@envoy\/\/include\//"@envoy\/\//g'
  for dir in common docs exe server; do
    find ./src ./test ./extensions \( -name \*.h -o -name \*.cc \) -print0 | xargs -0 sed -i "s/#include \"$dir\//#include \"source\/$dir\//g"
  done
}

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

# modify include paths to avoid overly long gcc command line
fix_include_paths

# wasm fix for s390x https://issues.redhat.com/browse/MAISTRA-2648
# the permanent fix is https://github.com/proxy-wasm/proxy-wasm-cpp-host/pull/198 (not a part of maistra/envoy yet)
patch -p1 -i maistra/ci/proxy-wasm-cpp-host.patch