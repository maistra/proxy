#!/bin/bash

set -e
set -o pipefail
set -x

# shellcheck disable=SC1091
source /opt/rh/gcc-toolset-9/enable

DIR=$(cd "$(dirname "$0")" ; pwd -P)

# shellcheck disable=SC1090
source "${DIR}/common.sh"

GCS_PROJECT=${GCS_PROJECT:-maistra-prow-testing}
ARTIFACTS_GCS_PATH=${ARTIFACTS_GCS_PATH:-gs://maistra-prow-testing/proxy}

gcloud auth activate-service-account --key-file="${GOOGLE_APPLICATION_CREDENTIALS}"
gcloud config set project "${GCS_PROJECT}"

# Build WASM extensions first
CC=clang CXX=clang++ bazel_build //extensions:stats.wasm
CC=clang CXX=clang++ bazel_build //extensions:metadata_exchange.wasm
CC=clang CXX=clang++ bazel_build //extensions:attributegen.wasm
CC=cc CXX=g++ bazel_build @envoy//test/tools/wee8_compile:wee8_compile_tool

CC=clang CXX=clang++ bazel-bin/external/envoy/test/tools/wee8_compile/wee8_compile_tool bazel-bin/extensions/stats.wasm bazel-bin/extensions/stats.compiled.wasm
CC=clang CXX=clang++ bazel-bin/external/envoy/test/tools/wee8_compile/wee8_compile_tool bazel-bin/extensions/metadata_exchange.wasm bazel-bin/extensions/metadata_exchange.compiled.wasm
CC=clang CXX=clang++ bazel-bin/external/envoy/test/tools/wee8_compile/wee8_compile_tool bazel-bin/extensions/attributegen.wasm bazel-bin/extensions/attributegen.compiled.wasm

# Build Envoy
CC=cc CXX=g++ bazel_build //src/envoy:envoy_tar

# Copy artifacts to GCS
SHA="$(git rev-parse --verify HEAD)"

# Envoy
gsutil cp bazel-bin/src/envoy/envoy_tar.tar.gz "${ARTIFACTS_GCS_PATH}/envoy-alpha-${SHA}.tar.gz"
gsutil cp "${ARTIFACTS_GCS_PATH}/envoy-alpha-${SHA}.tar.gz" "${ARTIFACTS_GCS_PATH}/envoy-centos-alpha-${SHA}.tar.gz"

# WASM extensions
gsutil cp bazel-bin/extensions/stats.wasm "${ARTIFACTS_GCS_PATH}/stats-${SHA}.wasm"
gsutil cp bazel-bin/extensions/stats.compiled.wasm "${ARTIFACTS_GCS_PATH}/stats-${SHA}.compiled.wasm"

gsutil cp bazel-bin/extensions/metadata_exchange.wasm "${ARTIFACTS_GCS_PATH}/metadata_exchange-${SHA}.wasm"
gsutil cp bazel-bin/extensions/metadata_exchange.compiled.wasm "${ARTIFACTS_GCS_PATH}/metadata_exchange-${SHA}.compiled.wasm"

gsutil cp bazel-bin/extensions/attributegen.wasm "${ARTIFACTS_GCS_PATH}/attributegen-${SHA}.wasm"
gsutil cp bazel-bin/extensions/attributegen.compiled.wasm "${ARTIFACTS_GCS_PATH}/attributegen-${SHA}.compiled.wasm"
