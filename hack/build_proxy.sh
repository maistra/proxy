#!/usr/bin/env bash

set -euo pipefail

: "${ISTIO_GITHUB_REPO_URL:=https://github.com/maistra/istio.git}"
: "${ISTIO_GITHUB_BRANCH:=maistra-3.0}"
: "${PROXY_GITHUB_REPO_URL:=https://github.com/maistra/proxy.git}"
: "${PROXY_GITHUB_BRANCH:=maistra-3.0-pre}"

function download_envoy_binary() {
  if [ $# -ne 1 ]; then
    echo "Usage: download_envoy_binary workdir"
    exit 1
  fi

  local workdir="${1}"

  git clone --single-branch -b "${ISTIO_GITHUB_BRANCH}" "${ISTIO_GITHUB_REPO_URL}" "${workdir}/istio"
  cd "${workdir}/istio"
  make init
}

function get_sha() {
  if [ $# -ne 1 ]; then
    echo "Usage: get_sha file"
    exit 1
  fi
  
  local file="${1}"
  sha256sum "${file}" | awk '{print $1}'
}

function is_envoy_changed() {
  if [ $# -ne 2 ]; then
    echo "Usage: is_envoy_changed current_envoy downloaded_envoy"
    exit 1
  fi

  local cur_envoy="${1}"
  local dl_envoy="${2}"

  cur_envoy_sha="$(get_sha "${cur_envoy}")"
  dl_envoy_sha="$(get_sha "${dl_envoy}")"

  [ "${cur_envoy_sha}" != "${dl_envoy_sha}" ]
}

function push_proxy_binary() {
  if [ $# -ne 2 ]; then
    echo "Usage: push_proxy_binary workdir envoy_bin_path"
    exit 1
  fi
  local workdir="${1}"
  local envoy_bin_path="${2}"

  git clone --single-branch -b "${PROXY_GITHUB_BRANCH}" "${PROXY_GITHUB_REPO_URL}" "${workdir}/proxy"
  cd "${workdir}"
  if [ "$(tar tf "${workdir}/proxy/proxy.tar.gz")" == "envoy" ]; then
    tar xzf "${workdir}/proxy/proxy.tar.gz" #untar the current envoy binary into workdir
  fi

  if is_envoy_changed "${workdir}/envoy" "$(dirname "${envoy_bin_path}")/envoy"; then
    echo "Envoy changed"
    cd "$(dirname "${envoy_bin_path}")"
    tar czf proxy.tar.gz envoy
    mv -f proxy.tar.gz "${workdir}/proxy"
    cd "${workdir}/proxy"
    git commit -a -m"Update proxy"
    git push origin HEAD
  else
    echo "No change"
  fi
}

# MAIN
tmpdir=$(mktemp -d)

download_envoy_binary "${tmpdir}"
push_proxy_binary "${tmpdir}" "${tmpdir}"/istio/out/linux_amd64/envoy

rm -rf "${tmpdir}"