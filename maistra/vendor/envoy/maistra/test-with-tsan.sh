#!/bin/bash

# This script will execute Envoy's c++ unit and integration tests with
# thread sanitizer enabled. It may point out concurrency issues like
# races with respect to access to data shared across threads.

export SANITIZER=${SANITIZER:-tsan}
DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
source "$DIR/test-with-asan.sh"
