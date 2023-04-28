#!/bin/bash

# This script will execute Envoy's c++ unit and integration tests with
# memory sanitizer enabled. It may point out use of uninitialized memory.

export SANITIZER=${SANITIZER:-msan}
DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
source "$DIR/test-with-asan.sh"
