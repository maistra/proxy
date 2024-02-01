# Copyright 2018 The Bazel Authors. All rights reserved.
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
    "//go/private:common.bzl",
    "GO_TOOLCHAIN",
)
load(
    "//go/private:providers.bzl",
    "EXPORT_PATH",
    "GoArchive",
    "GoLibrary",
    "get_archive",
)
load(
    "//go/private/rules:transition.bzl",
    "go_tool_transition",
)

def _nogo_impl(ctx):
    if not ctx.attr.deps:
        # If there aren't any analyzers to run, don't generate a binary.
        # go_context will check for this condition.
        return None

    # Generate the source for the nogo binary.
    go = go_context(ctx)
    nogo_main = go.declare_file(go, path = "nogo_main.go")
    nogo_args = ctx.actions.args()
    nogo_args.add("gennogomain")
    nogo_args.add("-output", nogo_main)
    nogo_inputs = []
    analyzer_archives = [get_archive(dep) for dep in ctx.attr.deps]
    analyzer_importpaths = [archive.data.importpath for archive in analyzer_archives]
    nogo_args.add_all(analyzer_importpaths, before_each = "-analyzer_importpath")
    if ctx.file.config:
        nogo_args.add("-config", ctx.file.config)
        nogo_inputs.append(ctx.file.config)
    ctx.actions.run(
        inputs = nogo_inputs,
        outputs = [nogo_main],
        mnemonic = "GoGenNogo",
        executable = go.toolchain._builder,
        arguments = [nogo_args],
    )

    # Compile the nogo binary itself.
    nogo_library = GoLibrary(
        name = go.label.name + "~nogo",
        label = go.label,
        importpath = "nogomain",
        importmap = "nogomain",
        importpath_aliases = (),
        pathtype = EXPORT_PATH,
        is_main = True,
        resolve = None,
    )

    nogo_source = go.library_to_source(go, struct(
        srcs = [struct(files = [nogo_main])],
        embed = [ctx.attr._nogo_srcs],
        deps = analyzer_archives,
    ), nogo_library, False)
    _, executable, runfiles = go.binary(
        go,
        name = ctx.label.name,
        source = nogo_source,
    )
    return [DefaultInfo(
        files = depset([executable]),
        runfiles = runfiles,
        executable = executable,
    )]

_nogo = rule(
    implementation = _nogo_impl,
    attrs = {
        "deps": attr.label_list(
            providers = [GoArchive],
        ),
        "config": attr.label(
            allow_single_file = True,
        ),
        "_nogo_srcs": attr.label(
            default = "//go/tools/builders:nogo_srcs",
        ),
        "_cgo_context_data": attr.label(default = "//:cgo_context_data_proxy"),
        "_go_config": attr.label(default = "//:go_config"),
        "_stdlib": attr.label(default = "//:stdlib"),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    },
    toolchains = [GO_TOOLCHAIN],
    cfg = go_tool_transition,
)

def nogo(name, visibility = None, **kwargs):
    actual_name = "%s_actual" % name
    native.alias(
        name = name,
        actual = select({
            str(Label("//go/private:nogo_active")): actual_name,
            "//conditions:default": Label("//:default_nogo"),
        }),
        visibility = visibility,
    )

    # With --use_top_level_targets_for_symlinks, which is enabled by default in
    # Bazel 6.0.0, self-transitioning top-level targets prevent the bazel-bin
    # convenience symlink from being created. Since nogo targets are of this
    # type, their presence would trigger this behavior. Work around this by
    # excluding them from wildcards - they are still transitively built as a
    # tool dependency of every Go target.
    kwargs.setdefault("tags", [])
    if "manual" not in kwargs["tags"]:
        kwargs["tags"].append("manual")

    _nogo(
        name = actual_name,
        visibility = visibility,
        **kwargs
    )

def nogo_wrapper(**kwargs):
    if kwargs.get("vet"):
        kwargs["deps"] = kwargs.get("deps", []) + [
            Label("@org_golang_x_tools//go/analysis/passes/atomic:go_default_library"),
            Label("@org_golang_x_tools//go/analysis/passes/bools:go_default_library"),
            Label("@org_golang_x_tools//go/analysis/passes/buildtag:go_default_library"),
            Label("@org_golang_x_tools//go/analysis/passes/nilfunc:go_default_library"),
            Label("@org_golang_x_tools//go/analysis/passes/printf:go_default_library"),
        ]
        kwargs = {k: v for k, v in kwargs.items() if k != "vet"}
    nogo(**kwargs)
