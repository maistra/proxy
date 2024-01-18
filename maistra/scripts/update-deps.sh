#!/bin/bash
# Copyright (C) 2020 Red Hat, Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -e
set -o pipefail

export CC=clang CXX=clang++

function init(){
  ROOT_DIR="$(pwd)"

  OUTPUT_BASE="$(mktemp -d)"
  #OUTPUT_BASE=/home/jwendell/bazel-output-base
  VENDOR_DIR="${ROOT_DIR}/maistra/vendor"
  PATCH_DIR="${ROOT_DIR}/maistra/patches"
  BAZELRC="${ROOT_DIR}/maistra/bazelrc-vendor"

  rm -rf "${OUTPUT_BASE}" &&  mkdir -p "${OUTPUT_BASE}"
  rm -rf "${VENDOR_DIR}" &&  mkdir -p "${VENDOR_DIR}"
  : > "${BAZELRC}"


  IGNORE_LIST=(
        "bazel_tools"
        "envoy_api"
        "envoy_build_config"
        "local_config"
        "local_jdk"
        "bazel_gazelle_go"
        "openssl"
        "go_sdk"
        "remotejdk"
        "rust"
        "nodejs"
        "rules_foreign_cc_framework_toolchain_freebsd_commands"
        "rules_foreign_cc_framework_toolchain_macos_commands"
        "rules_foreign_cc_framework_toolchain_windows_commands"
        "emscripten"
  )
}

function error() {
  echo "$@"
  exit 1
}

function validate() {
  if [ ! -f "WORKSPACE" ]; then
    error "Must run in the envoy/proxy dir"
  fi
}

function contains () {
  local e match="$1"
  shift
  for e; do [[ "$match" == "$e"* ]] && return 0; done
  return 1
}

function copy_files() {
  local cp_flags
  for f in "${OUTPUT_BASE}"/external/*; do
    if [ -d "${f}" ]; then
      repo_name=$(basename "${f}")
      if contains "${repo_name}" "${IGNORE_LIST[@]}" ; then
        continue
      fi

      cp_flags="-rL"
      if [ "${repo_name}" == "emscripten_toolchain" ]; then
        cp_flags="-r"
      fi
      cp "${cp_flags}" "${f}" "${VENDOR_DIR}" || echo "Copy of ${f} failed. Ignoring..."
      echo "build --override_repository=${repo_name}=/work/maistra/vendor/${repo_name}" >> "${BAZELRC}"
    fi
  done 

  # OSSM-1931: Install acorn module in maistra/vendor
  # npm install acorn@8.8.0
  # npm always installs "acorn" in $WORKDIR/node_modules
  # move it in the vendor directory
  # mv node_modules/acorn maistra/vendor/acorn
  # /bin/rm -rf node_modules

  find "${VENDOR_DIR}" -name .git -type d -print0 | xargs -0 -r rm -rf
  find "${VENDOR_DIR}" -name .gitignore -type f -delete
  find "${VENDOR_DIR}" -name __pycache__ -type d -print0 | xargs -0 -r rm -rf
  find "${VENDOR_DIR}" -name '*.pyc' -delete
}

# function apply_patches() {
#    # Patch emsdk and net_zlib
#   pushd "${OUTPUT_BASE}/external/emsdk" 
#   patch -p1 -i "${PATCH_DIR}"/emsdk.patch
#   popd

#   pushd "${OUTPUT_BASE}/external/net_zlib"
#   patch -p1  -i "${PATCH_DIR}"/net_zlib.patch
#   popd
# }

function run_bazel() {
  # Fetch stats_plugin just to load emsdk and net_zlib dependencies
  # bazel --output_base="${OUTPUT_BASE}" fetch //extensions/stats:stats_plugin || true

  # Workaround to force fetch of rules_license
  bazel --output_base="${OUTPUT_BASE}" fetch @remote_java_tools//java_tools/zlib:zlib || true

  #apply_patches

  # Fetch all the rest and check everything using "build --nobuild "option
  #for config in s390x ppc x86_64; do
  for config in x86_64; do
    bazel --output_base="${OUTPUT_BASE}" build --nobuild --config="${config}" //src/... //test/...  //extensions/...
  done
 
}

function main() {
  validate
  init
  run_bazel
  copy_files

  echo
  echo "Done. Inspect the result with git status"
  echo
}

main
