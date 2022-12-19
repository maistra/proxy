#!/bin/bash

export CC=clang CXX=clang++

ARCH=$(uname -p)
if [ "${ARCH}" = "ppc64le" ]; then
  ARCH="ppc"
fi
export ARCH
export EMSCRIPTEN="/opt/emsdk"

OUTPUT_TO_IGNORE="\
INFO: From|\
Fixing bazel-out|\
cache:INFO:  - ok|\
processwrapper-sandbox.*execroot.*io_istio_proxy|\
proto is unused\
"

# Store in an env var the working directory absolute path
# (it is different in different build environments)
WORKDIR=$(pwd)
export WORKDIR

# NODE_PATH: searching path for Node.js (necessary for "acorn" module)
# WORKDIR  : working directory path passed to Bazel
COMMON_FLAGS="\
    --config=release \
    --config=${ARCH} \
    --action_env=EMSCRIPTEN=${EMSCRIPTEN} \
    --action_env=WORKDIR=${WORKDIR} \
    --action_env=NODE_PATH=${WORKDIR}/maistra/vendor/ \
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
sed -i "s|=/work/|=$(pwd)/|" maistra/bazelrc-vendor

# Create symbolic links to existent executables in the builder
mkdir -p "$WORKDIR/maistra/local/bin"
ln -f -s /bin/node "$WORKDIR/maistra/local/bin/node"
ln -f -s /usr/bin/wasm-emscripten-finalize "$WORKDIR/maistra/local/bin/wasm-emscripten-finalize"
ln -f -s /usr/bin/wasm-opt "$WORKDIR/maistra/local/bin/wasm-opt"
