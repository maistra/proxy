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

load(
    "//go/private:context.bzl",
    "go_context",
)
load(
    "//go/private:providers.bzl",
    "GoLibrary",
)

def _go_source_impl(ctx):
    """Implements the go_source() rule."""
    go = go_context(ctx)
    library = go.new_library(go)
    source = go.library_to_source(go, ctx.attr, library, ctx.coverage_instrumented())
    return [
        library,
        source,
        DefaultInfo(
            files = depset(source.srcs),
        ),
    ]

go_source = rule(
    implementation = _go_source_impl,
    attrs = {
        "data": attr.label_list(allow_files = True),
        "srcs": attr.label_list(allow_files = True),
        "deps": attr.label_list(providers = [GoLibrary]),
        "embed": attr.label_list(providers = [GoLibrary]),
        "gc_goopts": attr.string_list(),
        "_go_config": attr.label(default = "//:go_config"),
        "_cgo_context_data": attr.label(default = "//:cgo_context_data_proxy"),
    },
    toolchains = ["@io_bazel_rules_go//go:toolchain"],
)
# See go/core.rst#go_source for full documentation.
