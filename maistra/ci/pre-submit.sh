#!/bin/bash

set -e
set -o pipefail
set -x

# shellcheck disable=SC1091
source /opt/rh/gcc-toolset-9/enable

DIR=$(cd "$(dirname "$0")" ; pwd -P)

# shellcheck disable=SC1090
source "${DIR}/common.sh"

# Build WASM extensions first
CC=clang CXX=clang++ bazel_build //extensions:stats.wasm
CC=clang CXX=clang++ bazel_build //extensions:metadata_exchange.wasm
CC=clang CXX=clang++ bazel_build //extensions:attributegen.wasm
CC=cc CXX=g++ bazel_build @envoy//test/tools/wee8_compile:wee8_compile_tool

CC=clang CXX=clang++ bazel-bin/external/envoy/test/tools/wee8_compile/wee8_compile_tool bazel-bin/extensions/stats.wasm bazel-bin/extensions/stats.compiled.wasm
CC=clang CXX=clang++ bazel-bin/external/envoy/test/tools/wee8_compile/wee8_compile_tool bazel-bin/extensions/metadata_exchange.wasm bazel-bin/extensions/metadata_exchange.compiled.wasm
CC=clang CXX=clang++ bazel-bin/external/envoy/test/tools/wee8_compile/wee8_compile_tool bazel-bin/extensions/attributegen.wasm bazel-bin/extensions/attributegen.compiled.wasm

echo "WASM extensions built succesfully. Now building envoy binary."

# Build Envoy
CC=cc CXX=g++ bazel_build //src/envoy:envoy

echo "Build succeeded. Binary generated:"
bazel-bin/src/envoy/envoy --version

# Run tests
CC=cc CXX=g++ bazel_test //src/... //test/...

export GOPROXY=off
export ENVOY_PATH=bazel-bin/src/envoy/envoy
export GO111MODULE=on

go test ./...
WASM=true go test ./test/envoye2e/stats_plugin/...
