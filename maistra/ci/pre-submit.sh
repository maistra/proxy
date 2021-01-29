#!/bin/bash

set -e
set -o pipefail
set -x

# shellcheck disable=SC1091
source /opt/rh/gcc-toolset-9/enable

DIR=$(cd "$(dirname "$0")" ; pwd -P)

# shellcheck disable=SC1090
source "${DIR}/common.sh"

# Fix path to the vendor deps
sed -i "s|=/work/|=$(pwd)/|" maistra/bazelrc-vendor

# Build WASM extensions first
bazel_build //extensions:stats.wasm
bazel_build //extensions:metadata_exchange.wasm
bazel_build //extensions:attributegen.wasm
bazel_build @envoy//test/tools/wee8_compile:wee8_compile_tool

bazel-bin/external/envoy/test/tools/wee8_compile/wee8_compile_tool bazel-bin/extensions/stats.wasm bazel-bin/extensions/stats.compiled.wasm
bazel-bin/external/envoy/test/tools/wee8_compile/wee8_compile_tool bazel-bin/extensions/metadata_exchange.wasm bazel-bin/extensions/metadata_exchange.compiled.wasm
bazel-bin/external/envoy/test/tools/wee8_compile/wee8_compile_tool bazel-bin/extensions/attributegen.wasm bazel-bin/extensions/attributegen.compiled.wasm

echo "WASM extensions built succesfully. Now building envoy binary."

# Build Envoy
bazel_build //src/envoy:envoy

echo "Build succeeded. Binary generated:"
bazel-bin/src/envoy/envoy --version

# Run tests
bazel_test //src/... //test/...
env ENVOY_PATH=bazel-bin/src/envoy/envoy GO111MODULE=on go test ./...
env ENVOY_PATH=bazel-bin/src/envoy/envoy GO111MODULE=on WASM=true go test ./test/envoye2e/stats_plugin/...
