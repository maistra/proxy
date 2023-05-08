# @generated by @aspect_bazel_lib//lib/private:jq_toolchain.bzl

# Forward all the providers
def _resolved_toolchain_impl(ctx):
    toolchain_info = ctx.toolchains["@aspect_bazel_lib//lib:jq_toolchain_type"]
    return [
        toolchain_info,
        toolchain_info.default,
        toolchain_info.jqinfo,
        toolchain_info.template_variables,
    ]

# Copied from java_toolchain_alias
# https://cs.opensource.google/bazel/bazel/+/master:tools/jdk/java_toolchain_alias.bzl
resolved_toolchain = rule(
    implementation = _resolved_toolchain_impl,
    toolchains = ["@aspect_bazel_lib//lib:jq_toolchain_type"],
    incompatible_use_toolchain_transition = True,
)
