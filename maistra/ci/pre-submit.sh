#!/bin/bash

set -e
set -o pipefail
set -x

DIR=$(cd "$(dirname "$0")" ; pwd -P)

# shellcheck disable=SC1091
source "${DIR}/common.sh"

# Build Envoy
time bazel_build //:envoy

echo "Build succeeded. Binary generated:"
bazel-bin/envoy --version

# Run tests
time bazel_test //...

export ENVOY_PATH=bazel-bin/envoy
export GO111MODULE=on
export GOPATH=$HOME/go

time go test -p 1 -parallel 1 ./...
# export WASM=true
# time go test -p 1 -parallel 1 ./test/envoye2e/stats_plugin/...
