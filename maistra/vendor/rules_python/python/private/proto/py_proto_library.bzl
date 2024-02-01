# Copyright 2022 The Bazel Authors. All rights reserved.
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

"""The implementation of the `py_proto_library` rule and its aspect."""

load("@rules_proto//proto:defs.bzl", "ProtoInfo", "proto_common")
load("//python:defs.bzl", "PyInfo")

ProtoLangToolchainInfo = proto_common.ProtoLangToolchainInfo

_PyProtoInfo = provider(
    doc = "Encapsulates information needed by the Python proto rules.",
    fields = {
        "runfiles_from_proto_deps": """
            (depset[File]) Files from the transitive closure implicit proto
            dependencies""",
        "transitive_sources": """(depset[File]) The Python sources.""",
    },
)

def _filter_provider(provider, *attrs):
    return [dep[provider] for attr in attrs for dep in attr if provider in dep]

def _py_proto_aspect_impl(target, ctx):
    """Generates and compiles Python code for a proto_library.

    The function runs protobuf compiler on the `proto_library` target generating
    a .py file for each .proto file.

    Args:
      target: (Target) A target providing `ProtoInfo`. Usually this means a
         `proto_library` target, but not always; you must expect to visit
         non-`proto_library` targets, too.
      ctx: (RuleContext) The rule context.

    Returns:
      ([_PyProtoInfo]) Providers collecting transitive information about
      generated files.
    """

    _proto_library = ctx.rule.attr

    # Check Proto file names
    for proto in target[ProtoInfo].direct_sources:
        if proto.is_source and "-" in proto.dirname:
            fail("Cannot generate Python code for a .proto whose path contains '-' ({}).".format(
                proto.path,
            ))

    proto_lang_toolchain_info = ctx.attr._aspect_proto_toolchain[ProtoLangToolchainInfo]
    api_deps = [proto_lang_toolchain_info.runtime]

    generated_sources = []
    proto_info = target[ProtoInfo]
    if proto_info.direct_sources:
        # Generate py files
        generated_sources = proto_common.declare_generated_files(
            actions = ctx.actions,
            proto_info = proto_info,
            extension = "_pb2.py",
            name_mapper = lambda name: name.replace("-", "_").replace(".", "/"),
        )

        proto_common.compile(
            actions = ctx.actions,
            proto_info = proto_info,
            proto_lang_toolchain_info = proto_lang_toolchain_info,
            generated_files = generated_sources,
            plugin_output = ctx.bin_dir.path,
        )

    # Generated sources == Python sources
    python_sources = generated_sources

    deps = _filter_provider(_PyProtoInfo, getattr(_proto_library, "deps", []))
    runfiles_from_proto_deps = depset(
        transitive = [dep[DefaultInfo].default_runfiles.files for dep in api_deps] +
                     [dep.runfiles_from_proto_deps for dep in deps],
    )
    transitive_sources = depset(
        direct = python_sources,
        transitive = [dep.transitive_sources for dep in deps],
    )

    return [
        _PyProtoInfo(
            runfiles_from_proto_deps = runfiles_from_proto_deps,
            transitive_sources = transitive_sources,
        ),
    ]

_py_proto_aspect = aspect(
    implementation = _py_proto_aspect_impl,
    attrs = {
        "_aspect_proto_toolchain": attr.label(
            default = ":python_toolchain",
        ),
    },
    attr_aspects = ["deps"],
    required_providers = [ProtoInfo],
    provides = [_PyProtoInfo],
)

def _py_proto_library_rule(ctx):
    """Merges results of `py_proto_aspect` in `deps`.

    Args:
      ctx: (RuleContext) The rule context.
    Returns:
      ([PyInfo, DefaultInfo, OutputGroupInfo])
    """
    if not ctx.attr.deps:
        fail("'deps' attribute mustn't be empty.")

    pyproto_infos = _filter_provider(_PyProtoInfo, ctx.attr.deps)
    default_outputs = depset(
        transitive = [info.transitive_sources for info in pyproto_infos],
    )

    return [
        DefaultInfo(
            files = default_outputs,
            default_runfiles = ctx.runfiles(transitive_files = depset(
                transitive =
                    [default_outputs] +
                    [info.runfiles_from_proto_deps for info in pyproto_infos],
            )),
        ),
        OutputGroupInfo(
            default = depset(),
        ),
        PyInfo(
            transitive_sources = default_outputs,
            # Proto always produces 2- and 3- compatible source files
            has_py2_only_sources = False,
            has_py3_only_sources = False,
        ),
    ]

py_proto_library = rule(
    implementation = _py_proto_library_rule,
    doc = """
      Use `py_proto_library` to generate Python libraries from `.proto` files.

      The convention is to name the `py_proto_library` rule `foo_py_pb2`,
      when it is wrapping `proto_library` rule `foo_proto`.

      `deps` must point to a `proto_library` rule.

      Example:

```starlark
py_library(
    name = "lib",
    deps = [":foo_py_pb2"],
)

py_proto_library(
    name = "foo_py_pb2",
    deps = [":foo_proto"],
)

proto_library(
    name = "foo_proto",
    srcs = ["foo.proto"],
)
```""",
    attrs = {
        "deps": attr.label_list(
            doc = """
              The list of `proto_library` rules to generate Python libraries for.

              Usually this is just the one target: the proto library of interest.
              It can be any target providing `ProtoInfo`.""",
            providers = [ProtoInfo],
            aspects = [_py_proto_aspect],
        ),
    },
    provides = [PyInfo],
)
