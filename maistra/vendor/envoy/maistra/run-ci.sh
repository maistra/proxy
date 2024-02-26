#!/bin/bash

set -e
set -o pipefail
set -x

DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
source "$DIR/common.sh"

export BUILD_SCM_REVISION="Maistra PR #${PULL_NUMBER:-undefined}"
export BUILD_SCM_STATUS="SHA=${PULL_PULL_SHA:-undefined}"

# Build
time bazel build \
  ${COMMON_FLAGS} \
  //source/exe:envoy-static

echo "Build succeeded. Binary generated:"
bazel-bin/source/exe/envoy-static --version

# By default, `bazel test` command performs simultaneous
# build and test activity.
# The following build step helps reduce resources usage
# by compiling tests first.
# Build tests
time bazel build \
  ${COMMON_FLAGS} \
  --build_tests_only \
  -- \
  //test/... \
  -//test/extensions/listener_managers/listener_manager:listener_manager_impl_quic_only_test

# These tests can fail with TIME OUT if run as part of batch
extra_tests=(
    //test/exe:main_common_test \
    //test/server:guarddog_impl_test \
    //test/common/stream_info:filter_state_impl_test \
    //test/common/common:assert_enabled_in_release_test \
    //test/common/common:assert_test \
    //test/common/upstream:conn_pool_map_impl_test \
    //test/common/conn_pool:conn_pool_base_test \
    //test/common/http/http2:codec_impl_test \
    //test/extensions/filters/http/header_mutation:header_mutation_test \
    //test/extensions/filters/http/gcp_authn:gcp_authn_filter_test \
    //test/extensions/filters/http/ext_proc:filter_test \
    //test/extensions/filters/http/custom_response:config_test \
    //test/extensions/filters/http/ext_proc:streaming_integration_test \
    //test/extensions/filters/network/http_connection_manager:config_test )

EXCLUDE_BATCH_TESTS=""
for test in "${extra_tests[@]}"
do
  EXCLUDE_BATCH_TESTS+=" -${test}"
done

# Run tests without tests that timeout
time bazel test \
  ${COMMON_FLAGS} \
  --build_tests_only \
  -- \
  //test/... \
  -//test/extensions/listener_managers/listener_manager:listener_manager_impl_quic_only_test \
  $EXCLUDE_BATCH_TESTS

# Run tests that where timing out in batch
for test in "${extra_tests[@]}"
do
echo "Running Test " $test
time bazel test \
  ${COMMON_FLAGS} \
  --build_tests_only \
  -- \
  $test 
done

