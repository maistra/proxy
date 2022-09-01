#!/bin/bash

# Modified for OSSM-1931
#export ROOT_DIR=${EXT_BUILD_ROOT:-$(pwd -P)}
export ROOT_DIR=${EXT_BUILD_ROOT:-$(pwd)}
export EM_BIN_PATH=""
#export EMSCRIPTEN=$ROOT_DIR/$EM_BIN_PATH/emscripten
export EMSCRIPTEN="$ROOT_DIR/external/emscripten_bin_linux"
#export EM_CONFIG=$ROOT_DIR/$EM_CONFIG_PATH
export EM_CONFIG=$ROOT_DIR/$EM_CONFIG_PATH
