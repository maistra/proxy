#!/bin/bash

set -e
set -o pipefail
set -x

DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
source "$DIR/common.sh"

export BUILD_SCM_REVISION="Maistra PR #${PULL_NUMBER:-undefined}"
export BUILD_SCM_STATUS="SHA=${PULL_PULL_SHA:-undefined}"



# Run tests
time bazel test \
  ${COMMON_FLAGS} \
  --build_tests_only \
  --test_output=errors \
  --cache_test_results=no \
  -- \
  //test/extensions/common/async_files:async_file_handle_thread_pool_test \
  //test/common/signal:signals_test \
  -//test/server:listener_manager_impl_quic_only_test 
