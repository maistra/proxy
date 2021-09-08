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

function fix_include_paths() {
  local pwd=`pwd`
  pushd maistra/vendor/envoy

  patch -p1 < "$pwd/maistra/ci/modify-envoy-paths.patch"
  rm -rf envoy
  mv include/envoy ./
  find ./envoy ./test ./source ./tools -name BUILD -print0 | xargs -0 sed -i 's/"\/\/include\//"\/\//g'
  find ./source -name BUILD -print0 | xargs -0 sed -i 's/"@envoy\/\/include\//"@envoy\/\//g'
  for dir in common docs exe extensions server; do
    find ./envoy ./source ./test ./tools \( -name \*.h -o -name \*.cc -o -name \*.j2 \) -print0 | xargs -0 sed -i "/Common.pb.h/!s/#include \"$dir\//#include \"source\/$dir\//g"
  done

  popd

  patch -p1 < "$pwd/maistra/ci/modify-proxy-paths.patch"
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
