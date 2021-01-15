#!/bin/sh

# Run the given binary, setting up a required test environment variable.

set -ex

export USER_DEFINED_KEY=USER_DEFINED_VALUE

"$@"
