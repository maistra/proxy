# Copyright 2016 Google Inc. All Rights Reserved.
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
#
################################################################################
#
workspace(name = "io_istio_proxy")

# http_archive is not a native function since bazel 0.19
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_file")
load(
    "//bazel:repositories.bzl",
    "googletest_repositories",
    "istioapi_dependencies",
)

googletest_repositories()

istioapi_dependencies()

new_local_repository(
    name = "openssl",
    path = "/usr/lib64/",
    build_file = "openssl.BUILD"
)

# 1. Determine SHA256 `wget https://github.com/envoyproxy/envoy/archive/$COMMIT.tar.gz && sha256sum $COMMIT.tar.gz`
# 2. Update .bazelversion, envoy.bazelrc and .bazelrc if needed.
#
# Note: this is needed by release builder to resolve envoy dep sha to tag.
# Commit date: 2021-05-11
ENVOY_SHA = "5a5f131fc2e13c0ebdf77bbc064e503f248ff5d2"

ENVOY_SHA256 = "662a3acdf00419c3f2a71a79adae68b404f39bdbd2bb7ac59cef3522b914bb1a"

ENVOY_ORG = "maistra"

ENVOY_REPO = "envoy"

# To override with local envoy, just pass `--override_repository=envoy=/PATH/TO/ENVOY` to Bazel or
# persist the option in `user.bazelrc`.
http_archive(
    name = "envoy",
    sha256 = ENVOY_SHA256,
    strip_prefix = ENVOY_REPO + "-" + ENVOY_SHA,
    url = "https://github.com/" + ENVOY_ORG + "/" + ENVOY_REPO + "/archive/" + ENVOY_SHA + ".tar.gz",
)

load("@envoy//bazel:api_binding.bzl", "envoy_api_binding")

envoy_api_binding()

load("@envoy//bazel:api_repositories.bzl", "envoy_api_dependencies")

envoy_api_dependencies()

load("@envoy//bazel:repositories.bzl", "envoy_dependencies")

envoy_dependencies()

load("@envoy//bazel:repositories_extra.bzl", "envoy_dependencies_extra")

envoy_dependencies_extra()

load("@envoy//bazel:dependency_imports.bzl", "envoy_dependency_imports")

envoy_dependency_imports()


load("@rules_antlr//antlr:deps.bzl", "antlr_dependencies")

antlr_dependencies(471)

FLAT_BUFFERS_SHA = "a83caf5910644ba1c421c002ef68e42f21c15f9f"

load("//bazel:wasm.bzl", "wasm_dependencies")

wasm_dependencies()
