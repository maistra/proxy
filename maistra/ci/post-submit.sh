#!/bin/bash

set -e
set -u
set -o pipefail
set -x

DIR=$(cd $(dirname $0) ; pwd -P)
source "${DIR}/common.sh"

GCS_PROJECT=${GCS_PROJECT:-maistra-prow-testing}
ARTIFACTS_GCS_PATH=${ARTIFACTS_GCS_PATH:-gs://maistra-prow-testing/proxy}

gcloud auth activate-service-account --key-file="${GOOGLE_APPLICATION_CREDENTIALS}"
gcloud config set project "${GCS_PROJECT}"

# Fix path to the vendor deps
sed -i "s|=/work/|=$(pwd)/|" maistra/bazelrc-vendor

ARCH=$(uname -p)
if [ "${ARCH}" = "ppc64le" ]; then
  ARCH="ppc"
fi

# Build
bazel build \
  --config=release \
  --config=${ARCH} \
  --local_resources 12288,4.0,1.0 \
  --jobs=4 \
  --disk_cache=/bazel-cache \
  //src/envoy:envoy_tar \
  2>&1 | grep -v -E "${OUTPUT_TO_IGNORE}"

# Copy binary to GCS
SHA="$(git rev-parse --verify HEAD)"
gsutil cp bazel-bin/src/envoy/envoy_tar.tar.gz "${ARTIFACTS_GCS_PATH}/envoy-alpha-${SHA}.tar.gz"

# Workaround WASM limitations
gsutil cp "${ARTIFACTS_GCS_PATH}/metadata_exchange.wasm" "${ARTIFACTS_GCS_PATH}/metadata_exchange-${SHA}.wasm"
gsutil cp "${ARTIFACTS_GCS_PATH}/stats.wasm" "${ARTIFACTS_GCS_PATH}/stats-${SHA}.wasm"
