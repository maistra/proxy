#!/bin/bash

# Runs Bazel build commands over clippy rules, where some are expected
# to fail.
#
# Can be run from anywhere within the rules_rust workspace.

set -euo pipefail

# Executes a bazel build command and handles the return value, exiting
# upon seeing an error.
#
# Takes two arguments:
# ${1}: The expected return code.
# ${2}: The target within "//test/clippy" to be tested.
function check_build_result() {
  local ret=0
  echo -n "Testing ${2}... "
  (bazel build //test/clippy:"${2}" &> /dev/null) || ret="$?" && true
  if [[ "${ret}" -ne "${1}" ]]; then
    echo "FAIL: Unexpected return code [saw: ${ret}, want: ${1}] building target //test/clippy:${2}"
    echo "  Run \"bazel build //test/clippy:${2}\" to see the output"
    exit 1
  else
    echo "OK"
  fi
}

function test_all() {
  local -r BUILD_OK=0
  local -r BUILD_FAILED=1
  local -r TEST_FAIL=3

  check_build_result $BUILD_OK ok_binary_clippy
  check_build_result $BUILD_OK ok_library_clippy
  check_build_result $BUILD_OK ok_test_clippy
  check_build_result $BUILD_FAILED bad_binary_clippy
  check_build_result $BUILD_FAILED bad_library_clippy
  check_build_result $BUILD_FAILED bad_test_clippy
}

test_all
