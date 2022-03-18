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

load("@io_bazel_rules_go//go/private:common.bzl", "executable_extension")

# Change to trigger cache invalidation: 1

def _go_repository_cache_impl(ctx):
    if ctx.attr.go_sdk_name:
        go_sdk_name = ctx.attr.go_sdk_name
    else:
        host_platform = _detect_host_platform(ctx)
        matches = [
            name
            for name, platform in ctx.attr.go_sdk_info.items()
            if host_platform == platform or platform == "host"
        ]
        if len(matches) > 1:
            fail('gazelle found more than one suitable Go SDK ({}). Specify which one to use with gazelle_dependencies(go_sdk = "go_sdk").'.format(", ".join(matches)))
        if len(matches) == 0:
            fail('gazelle could not find a Go SDK. Specify which one to use with gazelle_dependencies(go_sdk = "go_sdk").')
        if len(matches) == 1:
            go_sdk_name = matches[0]

    go_sdk_label = Label("@" + go_sdk_name + "//:ROOT")

    go_root = str(ctx.path(go_sdk_label).dirname)
    go_path = str(ctx.path("."))
    go_cache = str(ctx.path("gocache"))
    if ctx.os.environ.get("GO_REPOSITORY_USE_HOST_CACHE", "") == "1":
        extension = executable_extension(ctx)
        go_tool = go_root + "/bin/go" + extension
        res = ctx.execute([go_tool, "env", "GOPATH"])
        if res.return_code:
            fail("failed to read go environment: " + res.stderr)
        if not res.stdout:
            fail("GOPATH must be set when GO_REPOSITORY_USE_HOST_CACHE is enabled.")
        go_path = res.stdout.strip()
        res = ctx.execute([go_tool, "env", "GOCACHE"])
        if res.return_code:
            fail("failed to read go environment: " + res.stderr)
        if not res.stdout:
            fail("GOCACHE must be set when GO_REPOSITORY_USE_HOST_CACHE is enabled.")
        go_cache = res.stdout.strip()

    env_tpl = """
GOROOT='{goroot}'
GOPATH='{gopath}'
GOCACHE='{gocache}'
"""
    env_content = env_tpl.format(
        goroot = go_root,
        gopath = go_path,
        gocache = go_cache,
    )
    ctx.file("go.env", env_content)
    ctx.file("BUILD.bazel", 'exports_files(["go.env"])')

go_repository_cache = repository_rule(
    _go_repository_cache_impl,
    attrs = {
        "go_sdk_name": attr.string(),
        "go_sdk_info": attr.string_dict(),
    },
    # Don't put anything in environ. If we switch between the host cache
    # and Bazel's cache, it shouldn't actually invalidate Bazel's cache.
)

def read_cache_env(ctx, path):
    contents = ctx.read(path)
    env = {}
    lines = contents.split("\n")
    for line in lines:
        line = line.strip()
        if line == "" or line.startswith("#"):
            continue
        k, sep, v = line.partition("=")
        if sep == "":
            fail("failed to parse cache environment")
        env[k] = v.strip("'")
    return env

# copied from rules_go. Keep in sync.
def _detect_host_platform(ctx):
    if ctx.os.name == "linux":
        host = "linux_amd64"
        res = ctx.execute(["uname", "-p"])
        if res.return_code == 0:
            uname = res.stdout.strip()
            if uname == "s390x":
                host = "linux_s390x"
            elif uname == "i686":
                host = "linux_386"

        # uname -p is not working on Aarch64 boards
        # or for ppc64le on some distros
        res = ctx.execute(["uname", "-m"])
        if res.return_code == 0:
            uname = res.stdout.strip()
            if uname == "aarch64":
                host = "linux_arm64"
            elif uname == "armv6l":
                host = "linux_arm"
            elif uname == "armv7l":
                host = "linux_arm"
            elif uname == "ppc64le":
                host = "linux_ppc64le"

        # Default to amd64 when uname doesn't return a known value.

    elif ctx.os.name == "mac os x":
        host = "darwin_amd64"
    elif ctx.os.name.startswith("windows"):
        host = "windows_amd64"
    elif ctx.os.name == "freebsd":
        host = "freebsd_amd64"
    else:
        fail("Unsupported operating system: " + ctx.os.name)

    return host
