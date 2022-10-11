#!/bin/bash

export ROOT_DIR=${EXT_BUILD_ROOT:-$(pwd)}
# Modified for OSSM-1931
# The variable is defined in the build script
#export EMSCRIPTEN="$ROOT_DIR/external/emscripten_bin_linux"
export EM_CONFIG=$ROOT_DIR/$EM_CONFIG_PATH
