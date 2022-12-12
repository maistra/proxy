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

function init(){
  ROOT_DIR="$(pwd)"

  OUTPUT_BASE="$(mktemp -d)"
  VENDOR_DIR="${ROOT_DIR}/maistra/vendor"
  BAZELRC="${ROOT_DIR}/maistra/bazelrc-vendor"

  rm -rf "${OUTPUT_BASE}" &&  mkdir -p "${OUTPUT_BASE}"
  rm -rf "${VENDOR_DIR}" &&  mkdir -p "${VENDOR_DIR}"
  : > "${BAZELRC}"


  IGNORE_LIST=(
        "bazel_tools"
        "envoy_api"
        "envoy_build_config"
        "local_config_cc"
        "local_jdk"
        "local_config_cc_toolchains"
        "local_config_platform"
        "local_config_sh"
        "local_config_xcode"
        "bazel_gazelle_go_repository_cache"
        "bazel_gazelle_go_repository_config"
        "bazel_gazelle_go_repository_tools"
        "openssl"
        "go_sdk"
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
  for e; do [[ "$e" == "$match" ]] && return 0; done
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

  find "${VENDOR_DIR}" -name .git -type d -print0 | xargs -0 -r rm -rf
  find "${VENDOR_DIR}" -name .gitignore -type f -delete
  find "${VENDOR_DIR}" -name '*.pyc' -delete
done
}

function run_bazel() {
  bazel --output_base="${OUTPUT_BASE}" fetch //...
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
