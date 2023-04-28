#!/bin/bash

# This script will execute Envoy's c++ unit and integration tests with
# address sanitizer enabled. It may point out issues related to both
# undefined behaviour as well as memory management issues (leaks).

# An argument can be passed to this script which when specified will replace
# the "//test/..." part we pass to bazel. This allows one to optionally
# specify which tests to run.

# Furthermore, while this defaults to asan, one can set the SANITIZER env var
# to specify other sanitizers, as support in our bazel setup. 
# As of writing this, known valid values are "tsan" and "msan".

DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
source "$DIR/common.sh"

SANITIZER=${SANITIZER:-asan}
BAZEL_TESTS=${@:-//test/...}

FLAGS="${COMMON_FLAGS} \
  --config=clang-${SANITIZER} \
  -c dbg \
  ${BAZEL_TESTS} \
"

# We build and test in separate steps as in the past that has been observed to help
# stabilize the build as well as test execution.
echo "Build tests with ${SANITIZER} enabled."
time bazel build $FLAGS
  
echo "Execute tests with ${SANITIZER} enabled."
time bazel test $FLAGS
