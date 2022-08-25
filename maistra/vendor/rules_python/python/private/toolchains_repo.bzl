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

"""Create a repository to hold the toolchains.

This follows guidance here:
https://docs.bazel.build/versions/main/skylark/deploying.html#registering-toolchains

The "complex computation" in our case is simply downloading large artifacts.
This guidance tells us how to avoid that: we put the toolchain targets in the
alias repository with only the toolchain attribute pointing into the
platform-specific repositories.
"""

load(
    "//python:versions.bzl",
    "LINUX_NAME",
    "MACOS_NAME",
    "PLATFORMS",
    "WINDOWS_NAME",
)

def _toolchains_repo_impl(rctx):
    build_content = """\
# Generated by toolchains_repo.bzl
#
# These can be registered in the workspace file or passed to --extra_toolchains
# flag. By default all these toolchains are registered by the
# python_register_toolchains macro so you don't normally need to interact with
# these targets.

"""

    for [platform, meta] in PLATFORMS.items():
        build_content += """\
# Bazel selects this toolchain to get a Python interpreter
# for executing build actions.
toolchain(
    name = "{platform}_toolchain",
    exec_compatible_with = {compatible_with},
    toolchain = "@{user_repository_name}_{platform}//:python_runtimes",
    toolchain_type = "@bazel_tools//tools/python:toolchain_type",
)
""".format(
            platform = platform,
            name = rctx.attr.name,
            user_repository_name = rctx.attr.user_repository_name,
            compatible_with = meta.compatible_with,
        )

    rctx.file("BUILD.bazel", build_content)

toolchains_repo = repository_rule(
    _toolchains_repo_impl,
    doc = "Creates a repository with toolchain definitions for all known platforms " +
          "which can be registered or selected.",
    attrs = {
        "user_repository_name": attr.string(doc = "what the user chose for the base name"),
    },
)

def _resolved_interpreter_os_alias_impl(rctx):
    (os_name, arch) = _host_os_arch(rctx)

    host_platform = None
    for platform, meta in PLATFORMS.items():
        if meta.os_name == os_name and meta.arch == arch:
            host_platform = platform
    if not host_platform:
        fail("No platform declared for host OS {} on arch {}".format(os_name, arch))

    is_windows = (os_name == WINDOWS_NAME)
    python3_binary_path = "python.exe" if is_windows else "bin/python3"

    # Base BUILD file for this repository.
    build_contents = """\
# Generated by python/repositories.bzl
package(default_visibility = ["//visibility:public"])
alias(name = "files",           actual = "@{py_repository}_{host_platform}//:files")
alias(name = "py3_runtime",     actual = "@{py_repository}_{host_platform}//:py3_runtime")
alias(name = "python_runtimes", actual = "@{py_repository}_{host_platform}//:python_runtimes")
alias(name = "python3",         actual = "@{py_repository}_{host_platform}//:{python3_binary_path}")
""".format(
        py_repository = rctx.attr.user_repository_name,
        host_platform = host_platform,
        python3_binary_path = python3_binary_path,
    )
    if not is_windows:
        build_contents += """\
alias(name = "pip",             actual = "@{py_repository}_{host_platform}//:bin/pip")
""".format(
            py_repository = rctx.attr.user_repository_name,
            host_platform = host_platform,
        )
    rctx.file("BUILD.bazel", build_contents)

    # Expose a Starlark file so rules can know what host platform we used and where to find an interpreter
    # when using repository_ctx.path, which doesn't understand aliases.
    rctx.file("defs.bzl", content = """\
# Generated by python/repositories.bzl
host_platform = "{host_platform}"
interpreter = "@{py_repository}_{host_platform}//:{python3_binary_path}"
""".format(
        py_repository = rctx.attr.user_repository_name,
        host_platform = host_platform,
        python3_binary_path = python3_binary_path,
    ))

resolved_interpreter_os_alias = repository_rule(
    _resolved_interpreter_os_alias_impl,
    doc = """Creates a repository with a shorter name meant for the host platform, which contains
    a BUILD.bazel file declaring aliases to the host platform's targets.
    """,
    attrs = {
        "user_repository_name": attr.string(
            mandatory = True,
            doc = "The base name for all created repositories, like 'python38'.",
        ),
    },
)

def _host_os_arch(rctx):
    """Infer the host OS name and arch from a repository context.

    Args:
        rctx: Bazel's repository_ctx.
    Returns:
        A tuple with the host OS name and arch.
    """
    os_name = rctx.os.name

    # We assume the arch for Windows is always x86_64.
    if "windows" in os_name.lower():
        arch = "x86_64"

        # Normalize the os_name. E.g. os_name could be "OS windows server 2019".
        os_name = WINDOWS_NAME
    else:
        # This is not ideal, but bazel doesn't directly expose arch.
        arch = rctx.execute(["uname", "-m"]).stdout.strip()

        # Normalize the os_name.
        if "mac" in os_name.lower():
            os_name = MACOS_NAME
        elif "linux" in os_name.lower():
            os_name = LINUX_NAME

    return (os_name, arch)
