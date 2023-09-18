#!/bin/bash

# This script must run in the istio source tree.

set -euo pipefail

: "${GITHUB_TOKEN:?GitHub token is required}"

# Fetch GitHub data
raw=$(curl -sSfLH "Authorization: token ${GITHUB_TOKEN}" https://api.github.com/user)
GITHUB_USER=$(echo "${raw}" | jq --raw-output .login)
GITHUB_EMAIL=$(echo "${raw}" | jq --raw-output .email)

if [ -z "${GITHUB_USER}" ] || [ -z "${GITHUB_EMAIL}" ]; then
  echo "Error fetching bot's data from GitHub"
  exit 1
fi


: "${PROXY_GITHUB_REPO_URL:=https://${GITHUB_USER}:${GITHUB_TOKEN}@github.com/maistra/proxy.git}"
: "${PROXY_GITHUB_BRANCH:=maistra-3.0-pre}"


PROXYDIR=$(mktemp -d)
trap 'rm -rf "${PROXYDIR}"' EXIT

# Get current istio proxy sha
ISTIO_PROXY_SHA="${ISTIO_PROXY_SHA:-$(grep PROXY_REPO_SHA istio.deps  -A 4 | grep lastStableSHA | cut -f 4 -d '"')}"

# Inspect what we currently have
git clone --depth=1 --single-branch -b "${PROXY_GITHUB_BRANCH}" "${PROXY_GITHUB_REPO_URL}" "${PROXYDIR}"
MAISTRA_PROXY_SHA="$(cat "${PROXYDIR}/proxy.sha")"

# Exit early if proxy hasn't changed
if [ "${MAISTRA_PROXY_SHA}" == "${ISTIO_PROXY_SHA}" ]; then
  echo "Proxy has not changed. Nothing to do."
  exit 0
fi

# Download latest proxy and override the current one
URL="https://storage.googleapis.com/istio-build/proxy/envoy-alpha-${ISTIO_PROXY_SHA}.tar.gz"
curl -fLSs --retry 5 --retry-delay 1 --retry-connrefused -o "${PROXYDIR}/proxy.tar.gz" "${URL}"

# Update the SHA
echo "${ISTIO_PROXY_SHA}" > "${PROXYDIR}/proxy.sha"

# Commit the changes
cd "${PROXYDIR}"
git add .
git -c "user.name=${GITHUB_USER}" -c "user.email=${GITHUB_EMAIL}" commit --author="${GITHUB_USER} <${GITHUB_EMAIL}>" --no-gpg-sign -m "Update proxy"
git push origin "${PROXY_GITHUB_BRANCH}"
