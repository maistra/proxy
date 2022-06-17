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
    "//:repositories.bzl",
    "googletest_repositories",
    "mixerapi_dependencies",
)

googletest_repositories()

mixerapi_dependencies()

new_local_repository(
    name = "openssl",
    path = "/usr/lib64/",
    build_file = "openssl.BUILD"
)

# 1. Determine SHA256 `wget https://github.com/maistra/envoy/archive/$COMMIT.tar.gz && sha256sum $COMMIT.tar.gz`
# 2. Update .bazelversion, envoy.bazelrc and .bazelrc if needed.
#
# commit date: 06/30/2020
# Used by scripts/generate-wasm.sh
ENVOY_SHA = "548f7b3fdab6c6a86c55c72b9abbfef793fee6a5"
 
ENVOY_SHA256 = "451a2808e2dfe7ebaaaa5c88a6a21f604b374bb59f2062b9232cf18394706c40"
 
ENVOY_ORG = "maistra"

ENVOY_REPO = "envoy"

# To override with local envoy, just pass `--override_repository=envoy=/PATH/TO/ENVOY` to Bazel or
# persist the option in `user.bazelrc`.
http_archive(
    name = ENVOY_REPO,
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

load("@envoy//bazel:dependency_imports.bzl", "envoy_dependency_imports")

envoy_dependency_imports()

load("@rules_antlr//antlr:deps.bzl", "antlr_dependencies")

antlr_dependencies(472)

FLAT_BUFFERS_SHA = "a83caf5910644ba1c421c002ef68e42f21c15f9f"

http_archive(
    name = "com_github_google_flatbuffers",
    sha256 = "b8efbc25721e76780752bad775a97c3f77a0250271e2db37fc747b20e8b0f24a",
    strip_prefix = "flatbuffers-" + FLAT_BUFFERS_SHA,
    url = "https://github.com/google/flatbuffers/archive/" + FLAT_BUFFERS_SHA + ".tar.gz",
)

http_file(
    name = "com_github_nlohmann_json_single_header",
    sha256 = "3b5d2b8f8282b80557091514d8ab97e27f9574336c804ee666fda673a9b59926",
    urls = [
        "https://github.com/nlohmann/json/releases/download/v3.7.3/json.hpp",
    ],
)
