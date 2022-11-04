# Copyright 2019 The Bazel Authors. All rights reserved.
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

"""run_binary implementation"""

load("@bazel_skylib//lib:dicts.bzl", "dicts")
load(":expand_locations.bzl", "expand_locations")
load(":expand_variables.bzl", "expand_variables")
load("//lib:stamping.bzl", "STAMP_ATTRS", "maybe_stamp")

def _impl(ctx):
    tool_as_list = [ctx.attr.tool]
    tool_inputs, tool_input_mfs = ctx.resolve_tools(tools = tool_as_list)
    args = ctx.actions.args()

    outputs = []
    outputs.extend(ctx.outputs.outs)
    for _out_dir in ctx.attr.out_dirs:
        out_dir = ctx.actions.declare_directory(_out_dir)
        for output in outputs:
            if output.path.startswith(out_dir.path + "/"):
                fail("output {} is nested within output directory {}; outputs cannot be nested within each other!".format(output.path, out_dir.path))
            if output.is_directory and out_dir.path.startswith(output.path + "/"):
                fail("output directory {} is nested within output directory {}; outputs cannot be nested within each other!".format(out_dir.path, output.path))
        outputs.append(out_dir)
    if len(outputs) < 1:
        fail("""\
ERROR: target {target} is not configured to produce any outputs.

Bazel only executes actions when their outputs are required, so it's never correct to create an action with no outputs.

Possible fixes:
- Predict what outputs are created, and list them in the outs and out_dirs attributes.
- If {rule_kind} is a binary, and you meant to run it for its side-effects,
  then call it directly with `bazel run` and don't wrap it in a run_binary rule.
""".format(
            target = str(ctx.label),
            rule_kind = str(ctx.attr.tool.label),
        ))

    # `expand_locations(...).split(" ")` is a work-around https://github.com/bazelbuild/bazel/issues/10309
    # _expand_locations returns an array of args to support $(execpaths) expansions.
    # TODO: If the string has intentional spaces or if one or more of the expanded file
    # locations has a space in the name, we will incorrectly split it into multiple arguments
    for a in ctx.attr.args:
        args.add_all([expand_variables(ctx, e, outs = outputs) for e in expand_locations(ctx, a, ctx.attr.srcs).split(" ")])
    envs = {}
    for k, v in ctx.attr.env.items():
        envs[k] = " ".join([expand_variables(ctx, e, outs = outputs, attribute_name = "env") for e in expand_locations(ctx, v, ctx.attr.srcs).split(" ")])

    stamp = maybe_stamp(ctx)
    if stamp:
        inputs = ctx.files.srcs + [stamp.volatile_status_file, stamp.stable_status_file]
        envs["BAZEL_STABLE_STATUS_FILE"] = stamp.stable_status_file.path
        envs["BAZEL_VOLATILE_STATUS_FILE"] = stamp.volatile_status_file.path
    else:
        inputs = ctx.files.srcs

    ctx.actions.run(
        outputs = outputs,
        inputs = inputs,
        tools = tool_inputs,
        executable = ctx.executable.tool,
        arguments = [args],
        mnemonic = ctx.attr.mnemonic if ctx.attr.mnemonic else None,
        progress_message = ctx.attr.progress_message if ctx.attr.progress_message else None,
        execution_requirements = ctx.attr.execution_requirements if ctx.attr.execution_requirements else None,
        use_default_shell_env = False,
        env = dicts.add(ctx.configuration.default_shell_env, envs),
        input_manifests = tool_input_mfs,
    )
    return DefaultInfo(
        files = depset(outputs),
        runfiles = ctx.runfiles(files = outputs),
    )

_run_binary = rule(
    implementation = _impl,
    attrs = dict({
        "tool": attr.label(
            executable = True,
            allow_files = True,
            mandatory = True,
            cfg = "exec",
        ),
        "env": attr.string_dict(),
        "srcs": attr.label_list(
            allow_files = True,
        ),
        "out_dirs": attr.string_list(),
        "outs": attr.output_list(),
        "args": attr.string_list(),
        "mnemonic": attr.string(),
        "progress_message": attr.string(),
        "execution_requirements": attr.string_dict(),
    }, **STAMP_ATTRS),
)

def run_binary(
        name,
        tool,
        srcs = [],
        args = [],
        env = {},
        outs = [],
        out_dirs = [],
        mnemonic = "RunBinary",
        progress_message = None,
        execution_requirements = None,
        stamp = 0,
        # TODO: remove output_dir in 2.x release
        output_dir = False,
        **kwargs):
    """Runs a binary as a build action.

    This rule does not require Bash (unlike `native.genrule`).

    Args:
        name: The target name

        tool: The tool to run in the action.

            Must be the label of a *_binary rule of a rule that generates an executable file, or of
            a file that can be executed as a subprocess (e.g. an .exe or .bat file on Windows or a
            binary with executable permission on Linux). This label is available for `$(location)`
            expansion in `args` and `env`.
        srcs: Additional inputs of the action.

            These labels are available for `$(location)` expansion in `args` and `env`.

        args: Command line arguments of the binary.

            Subject to `$(location)` and makevar expansions.

        env: Environment variables of the action.

            Subject to `$(location)` and makevar expansions.

        outs: Output files generated by the action.

            These labels are available for `$(location)` expansion in `args` and `env`.

            Output files cannot be nested within output directories in out_dirs.

        out_dirs: Output directories generated by the action.

            These labels are _not_ available for `$(location)` expansion in `args` and `env` sincew
            they are not pre-declared labels created via attr.output_list(). Output directories are
            declared instead by `ctx.actions.declare_directory`.

            Output directories cannot be nested within other output directories in out_dirs.

        mnemonic: A one-word description of the action, for example, CppCompile or GoLink.

        progress_message: Progress message to show to the user during the build, for example,
            "Compiling foo.cc to create foo.o". The message may contain %{label}, %{input}, or
            %{output} patterns, which are substituted with label string, first input, or output's
            path, respectively. Prefer to use patterns instead of static strings, because the former
            are more efficient.

        execution_requirements: Information for scheduling the action.

            For example,

            ```
            execution_requirements = {
                "no-cache": "1",
            },
            ```

            See https://docs.bazel.build/versions/main/be/common-definitions.html#common.tags for useful keys.

        output_dir: If set to True then an output directory named the same as the target name
            is added to out_dirs.

            Deprecated. For backward compatability with @aspect_bazel_lib 1.x. Use out_dirs instead.

        stamp: Whether to include build status files as inputs to the tool. Possible values:

            - `stamp = 0` (default): Never include build status files as inputs to the tool.
                This gives good build result caching.
                Most tools don't use the status files, so including them in `--stamp` builds makes those
                builds have many needless cache misses.
                (Note: this default is different from most rules with an integer-typed `stamp` attribute.)
            - `stamp = 1`: Always include build status files as inputs to the tool, even in
                [--nostamp](https://docs.bazel.build/versions/main/user-manual.html#flag--stamp) builds.
                This setting should be avoided, since it is non-deterministic.
                It potentially causes remote cache misses for the target and
                any downstream actions that depend on the result.
            - `stamp = -1`: Inclusion of build status files as inputs is controlled by the
                [--[no]stamp](https://docs.bazel.build/versions/main/user-manual.html#flag--stamp) flag.
                Stamped targets are not rebuilt unless their dependencies change.

            When stamping is enabled, an additional two environment variables will be set for the action:
                - `BAZEL_STABLE_STATUS_FILE`
                - `BAZEL_VOLATILE_STATUS_FILE`

            These files can be read and parsed by the action, for example to pass some values to a linker.

        **kwargs: Additional arguments
    """
    _run_binary(
        name = name,
        tool = tool,
        srcs = srcs,
        args = args,
        env = env,
        outs = outs,
        out_dirs = out_dirs + ([name] if output_dir else []),
        mnemonic = mnemonic,
        progress_message = progress_message,
        execution_requirements = execution_requirements,
        stamp = stamp,
        **kwargs
    )
