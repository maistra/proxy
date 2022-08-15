#!/bin/bash

set -e
set -o pipefail
set -x

DIR=$(cd "$(dirname "$0")" ; pwd -P)

# shellcheck disable=SC1091
source "${DIR}/common.sh"

# Build WASM extensions first
time bazel_build //extensions:stats.wasm
time bazel_build //extensions:metadata_exchange.wasm
time bazel_build //extensions:attributegen.wasm
time bazel_build @envoy//test/tools/wee8_compile:wee8_compile_tool

bazel-bin/external/envoy/test/tools/wee8_compile/wee8_compile_tool bazel-bin/extensions/stats.wasm bazel-bin/extensions/stats.compiled.wasm
bazel-bin/external/envoy/test/tools/wee8_compile/wee8_compile_tool bazel-bin/extensions/metadata_exchange.wasm bazel-bin/extensions/metadata_exchange.compiled.wasm
bazel-bin/external/envoy/test/tools/wee8_compile/wee8_compile_tool bazel-bin/extensions/attributegen.wasm bazel-bin/extensions/attributegen.compiled.wasm

echo "WASM extensions built succesfully. Now building envoy binary."

# Build Envoy
time bazel_build //src/envoy:envoy

echo "Build succeeded. Binary generated:"
bazel-bin/src/envoy/envoy --version

# Run tests
time bazel_test //src/... //test/...

export ENVOY_PATH=bazel-bin/src/envoy/envoy
export GO111MODULE=on
export GOPATH=$HOME/go

time go test ./...
export WASM=true
time go test ./test/envoye2e/stats_plugin/...
