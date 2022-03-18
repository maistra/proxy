# Copyright 2017 The Bazel Authors. All rights reserved.
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
package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

exports_files([
    "LICENSE",
    "version.bzl",
])

filegroup(
    name = "distribution",
    srcs = [
        "BUILD",
        "LICENSE",
        "internal_deps.bzl",
        "internal_setup.bzl",
        "//python:distribution",
        "//python/pip_install:distribution",
        "//tools:distribution",
    ],
    visibility = ["//distro:__pkg__"],
)

# Reexport of all bzl files used to allow downstream rules to generate docs
# without shipping with a dependency on Skylib
filegroup(
    name = "bzl",
    srcs = [
        "//python/pip_install:bzl",
        "//python:bzl",
        # Requires Bazel 0.29 onward for public visibility of these .bzl files.
        "@bazel_tools//tools/python:private/defs.bzl",
        "@bazel_tools//tools/python:python_version.bzl",
        "@bazel_tools//tools/python:srcs_version.bzl",
        "@bazel_tools//tools/python:toolchain.bzl",
        "@bazel_tools//tools/python:utils.bzl",
    ],
    visibility = ["//visibility:public"],
)
