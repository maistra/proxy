"Rules for working with transitions."

def _transition_platform_impl(_, attr):
    return {"//command_line_option:platforms": str(attr.target_platform)}

# Transition from any input configuration to one that includes the
# --platforms command-line flag.
_transition_platform = transition(
    implementation = _transition_platform_impl,
    inputs = [],
    outputs = ["//command_line_option:platforms"],
)

def _platform_transition_filegroup_impl(ctx):
    files = []
    runfiles = ctx.runfiles()
    for src in ctx.attr.srcs:
        files.append(src[DefaultInfo].files)

    runfiles = runfiles.merge_all([src[DefaultInfo].default_runfiles for src in ctx.attr.srcs])
    return [DefaultInfo(
        files = depset(transitive = files),
        runfiles = runfiles,
    )]

platform_transition_filegroup = rule(
    _platform_transition_filegroup_impl,
    attrs = {
        # Required to Opt-in to the transitions feature.
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
        "target_platform": attr.label(
            doc = "The target platform to transition the srcs.",
            mandatory = True,
        ),
        "srcs": attr.label_list(
            allow_empty = False,
            cfg = _transition_platform,
            doc = "The input to be transitioned to the target platform.",
        ),
    },
    doc = "Transitions the srcs to use the provided platform. The filegroup will contain artifacts for the target platform.",
)
