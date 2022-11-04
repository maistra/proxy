# Copyright 2014 The Bazel Authors. All rights reserved.
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
    "//go/private:common.bzl",
    "executable_path",
)
load(
    "//go/private:nogo.bzl",
    "go_register_nogo",
)
load(
    "//go/private:platforms.bzl",
    "generate_toolchain_names",
)

MIN_SUPPORTED_VERSION = (1, 14, 0)

def _go_host_sdk_impl(ctx):
    goroot = _detect_host_sdk(ctx)
    platform = _detect_sdk_platform(ctx, goroot)
    version = _detect_sdk_version(ctx, goroot)
    _sdk_build_file(ctx, platform, version)
    _local_sdk(ctx, goroot)

_go_host_sdk = repository_rule(
    implementation = _go_host_sdk_impl,
    environ = ["GOROOT"],
)

def go_host_sdk(name, register_toolchains = True, **kwargs):
    _go_host_sdk(name = name, **kwargs)
    if register_toolchains:
        _register_toolchains(name)

def _go_download_sdk_impl(ctx):
    if not ctx.attr.goos and not ctx.attr.goarch:
        goos, goarch = _detect_host_platform(ctx)
    else:
        if not ctx.attr.goos:
            fail("goarch set but goos not set")
        if not ctx.attr.goarch:
            fail("goos set but goarch not set")
        goos, goarch = ctx.attr.goos, ctx.attr.goarch
    platform = goos + "_" + goarch

    version = ctx.attr.version
    sdks = ctx.attr.sdks

    if not sdks:
        # If sdks was unspecified, download a full list of files.
        # If version was unspecified, pick the latest version.
        # Even if version was specified, we need to download the file list
        # to find the SHA-256 sum. If we don't have it, Bazel won't cache
        # the downloaded archive.
        if not version:
            ctx.report_progress("Finding latest Go version")
        else:
            ctx.report_progress("Finding Go SHA-256 sums")
        ctx.download(
            url = [
                "https://golang.org/dl/?mode=json&include=all",
                "https://golang.google.cn/dl/?mode=json&include=all",
            ],
            output = "versions.json",
        )

        data = ctx.read("versions.json")
        sdks_by_version = _parse_versions_json(data)

        if not version:
            highest_version = None
            for v in sdks_by_version.keys():
                pv = _parse_version(v)
                if not pv or _version_is_prerelease(pv):
                    # skip parse errors and pre-release versions
                    continue
                if not highest_version or _version_less(highest_version, pv):
                    highest_version = pv
            if not highest_version:
                fail("did not find any Go versions in https://golang.org/dl/?mode=json")
            version = _version_string(highest_version)
        if version not in sdks_by_version:
            fail("did not find version {} in https://golang.org/dl/?mode=json".format(version))
        sdks = sdks_by_version[version]

    if platform not in sdks:
        fail("unsupported platform {}".format(platform))
    filename, sha256 = sdks[platform]
    _remote_sdk(ctx, [url.format(filename) for url in ctx.attr.urls], ctx.attr.strip_prefix, sha256)

    detected_version = _detect_sdk_version(ctx, ".")
    _sdk_build_file(ctx, platform, detected_version)

    if not ctx.attr.sdks and not ctx.attr.version:
        # Returning this makes Bazel print a message that 'version' must be
        # specified for a reproducible build.
        return {
            "name": ctx.attr.name,
            "goos": ctx.attr.goos,
            "goarch": ctx.attr.goarch,
            "sdks": ctx.attr.sdks,
            "urls": ctx.attr.urls,
            "version": version,
            "strip_prefix": ctx.attr.strip_prefix,
        }
    return None

_go_download_sdk = repository_rule(
    implementation = _go_download_sdk_impl,
    attrs = {
        "goos": attr.string(),
        "goarch": attr.string(),
        "sdks": attr.string_list_dict(),
        "urls": attr.string_list(default = ["https://dl.google.com/go/{}"]),
        "version": attr.string(),
        "strip_prefix": attr.string(default = "go"),
    },
)

def go_download_sdk(name, register_toolchains = True, **kwargs):
    _go_download_sdk(name = name, **kwargs)
    if register_toolchains:
        _register_toolchains(name)

def _go_local_sdk_impl(ctx):
    goroot = ctx.attr.path
    platform = _detect_sdk_platform(ctx, goroot)
    version = _detect_sdk_version(ctx, goroot)
    _sdk_build_file(ctx, platform, version)
    _local_sdk(ctx, goroot)

_go_local_sdk = repository_rule(
    implementation = _go_local_sdk_impl,
    attrs = {
        "path": attr.string(),
    },
)

def go_local_sdk(name, register_toolchains = True, **kwargs):
    _go_local_sdk(name = name, **kwargs)
    if register_toolchains:
        _register_toolchains(name)

def _go_wrap_sdk_impl(ctx):
    if not ctx.attr.root_file and not ctx.attr.root_files:
        fail("either root_file or root_files must be provided")
    if ctx.attr.root_file and ctx.attr.root_files:
        fail("root_file and root_files cannot be both provided")
    if ctx.attr.root_file:
        root_file = ctx.attr.root_file
    else:
        goos, goarch = _detect_host_platform(ctx)
        platform = goos + "_" + goarch
        if platform not in ctx.attr.root_files:
            fail("unsupported platform {}".format(platform))
        root_file = Label(ctx.attr.root_files[platform])
    goroot = str(ctx.path(root_file).dirname)
    platform = _detect_sdk_platform(ctx, goroot)
    version = _detect_sdk_version(ctx, goroot)
    _sdk_build_file(ctx, platform, version)
    _local_sdk(ctx, goroot)

_go_wrap_sdk = repository_rule(
    implementation = _go_wrap_sdk_impl,
    attrs = {
        "root_file": attr.label(
            mandatory = False,
            doc = "A file in the SDK root direcotry. Used to determine GOROOT.",
        ),
        "root_files": attr.string_dict(
            mandatory = False,
            doc = "A set of mappings from the host platform to a file in the SDK's root directory",
        ),
    },
)

def go_wrap_sdk(name, register_toolchains = True, **kwargs):
    _go_wrap_sdk(name = name, **kwargs)
    if register_toolchains:
        _register_toolchains(name)

def _register_toolchains(repo):
    labels = [
        "@{}//:{}".format(repo, name)
        for name in generate_toolchain_names()
    ]
    native.register_toolchains(*labels)

def _remote_sdk(ctx, urls, strip_prefix, sha256):
    if len(urls) == 0:
        fail("no urls specified")
    ctx.report_progress("Downloading and extracting Go toolchain")
    if urls[0].endswith(".tar.gz"):
        # BUG(#2771): Use a system tool to extract the archive instead of
        # Bazel's implementation. With some configurations (macOS + Docker +
        # some particular file system binding), Bazel's implementation rejects
        # files with invalid unicode names. Go has at least one test case with a
        # file like this, but we haven't been able to reproduce the failure, so
        # instead, we use this workaround.
        if strip_prefix != "go":
            fail("strip_prefix not supported")
        ctx.download(
            url = urls,
            sha256 = sha256,
            output = "go_sdk.tar.gz",
        )
        res = ctx.execute(["tar", "-xf", "go_sdk.tar.gz", "--strip-components=1"])
        if res.return_code:
            fail("error extracting Go SDK:\n" + res.stdout + res.stderr)
        ctx.delete("go_sdk.tar.gz")
    else:
        ctx.download_and_extract(
            url = urls,
            stripPrefix = strip_prefix,
            sha256 = sha256,
        )

def _local_sdk(ctx, path):
    for entry in ["src", "pkg", "bin", "lib"]:
        ctx.symlink(path + "/" + entry, entry)

def _sdk_build_file(ctx, platform, version):
    ctx.file("ROOT")
    goos, _, goarch = platform.partition("_")

    pv = _parse_version(version)
    if pv == None or len(pv) < 3:
        fail("error parsing sdk version: " + version)
    major, minor, patch = pv[0], pv[1], pv[2]
    prerelease = pv[3] if len(pv) > 3 else ""

    ctx.template(
        "BUILD.bazel",
        Label("//go/private:BUILD.sdk.bazel"),
        executable = False,
        substitutions = {
            "{goos}": goos,
            "{goarch}": goarch,
            "{exe}": ".exe" if goos == "windows" else "",
            "{rules_go_repo_name}": "io_bazel_rules_go",
            "{major_version}": str(major),
            "{minor_version}": str(minor),
            "{patch_version}": str(patch),
            "{prerelease_suffix}": prerelease,
        },
    )

def _detect_host_platform(ctx):
    goos = ctx.os.name
    if goos == "mac os x":
        goos = "darwin"
    elif goos.startswith("windows"):
        goos = "windows"

    goarch = ctx.os.arch
    if goarch == "aarch64":
        goarch = "arm64"
    elif goarch == "x86_64":
        goarch = "amd64"

    return goos, goarch

def _detect_host_sdk(ctx):
    root = "@invalid@"
    if "GOROOT" in ctx.os.environ:
        return ctx.os.environ["GOROOT"]
    res = ctx.execute([executable_path(ctx, "go"), "env", "GOROOT"])
    if res.return_code:
        fail("Could not detect host go version")
    root = res.stdout.strip()
    if not root:
        fail("host go version failed to report it's GOROOT")
    return root

def _detect_sdk_platform(ctx, goroot):
    path = goroot + "/pkg/tool"
    res = ctx.execute(["ls", path])
    if res.return_code != 0:
        fail("Could not detect SDK platform: unable to list %s: %s" % (path, res.stderr))

    platforms = []
    for f in res.stdout.strip().split("\n"):
        if f.find("_") >= 0:
            platforms.append(f)

    if len(platforms) == 0:
        fail("Could not detect SDK platform: found no platforms in %s" % path)
    if len(platforms) > 1:
        fail("Could not detect SDK platform: found multiple platforms %s in %s" % (platforms, path))
    return platforms[0]

def _detect_sdk_version(ctx, goroot):
    path = goroot + "/VERSION"
    version_contents = ctx.read(path)

    # VERSION file has version prefixed by go, eg. go1.18.3
    version = version_contents[2:]
    if _parse_version(version) == None:
        fail("Could not parse SDK version from version file (%s): %s" % (path, version_contents))
    return version

def _parse_versions_json(data):
    """Parses version metadata returned by golang.org.

    Args:
        data: the contents of the file downloaded from
            https://golang.org/dl/?mode=json. We assume the file is valid
            JSON, is spaced and indented, and is in a particular format.

    Return:
        A dict mapping version strings (like "1.15.5") to dicts mapping
        platform names (like "linux_amd64") to pairs of filenames
        (like "go1.15.5.linux-amd64.tar.gz") and hex-encoded SHA-256 sums.
    """
    sdks = json.decode(data)
    return {
        sdk["version"][len("go"):]: {
            "%s_%s" % (file["os"], file["arch"]): (
                file["filename"],
                file["sha256"],
            )
            for file in sdk["files"]
            if file["kind"] == "archive"
        }
        for sdk in sdks
    }

def _parse_version(version):
    """Parses a version string like "1.15.5" and returns a tuple of numbers or None"""
    l, r = 0, 0
    parsed = []
    for c in version.elems():
        if c == ".":
            if l == r:
                # empty component
                return None
            parsed.append(int(version[l:r]))
            r += 1
            l = r
            continue

        if c.isdigit():
            r += 1
            continue

        # pre-release suffix
        break

    if l == r:
        # empty component
        return None
    parsed.append(int(version[l:r]))
    if len(parsed) == 2:
        # first minor version, like (1, 15)
        parsed.append(0)
    if len(parsed) != 3:
        # too many or too few components
        return None
    if r < len(version):
        # pre-release suffix
        parsed.append(version[r:])
    return tuple(parsed)

def _version_is_prerelease(v):
    return len(v) > 3

def _version_less(a, b):
    if a[:3] < b[:3]:
        return True
    if a[:3] > b[:3]:
        return False
    if len(a) > len(b):
        return True
    if len(a) < len(b) or len(a) == 3:
        return False
    return a[3:] < b[3:]

def _version_string(v):
    suffix = v[3] if _version_is_prerelease(v) else ""
    if v[-1] == 0:
        v = v[:-1]
    return ".".join([str(n) for n in v]) + suffix

def go_register_toolchains(version = None, nogo = None, go_version = None):
    """See /go/toolchains.rst#go-register-toolchains for full documentation."""
    if not version:
        version = go_version  # old name

    sdk_kinds = ("_go_download_sdk", "_go_host_sdk", "_go_local_sdk", "_go_wrap_sdk")
    existing_rules = native.existing_rules()
    sdk_rules = [r for r in existing_rules.values() if r["kind"] in sdk_kinds]
    if len(sdk_rules) == 0 and "go_sdk" in existing_rules:
        # may be local_repository in bazel_tests.
        sdk_rules.append(existing_rules["go_sdk"])

    if version and len(sdk_rules) > 0:
        fail("go_register_toolchains: version set after go sdk rule declared ({})".format(", ".join([r["name"] for r in sdk_rules])))
    if len(sdk_rules) == 0:
        if not version:
            fail('go_register_toolchains: version must be a string like "1.15.5" or "host"')
        elif version == "host":
            go_host_sdk(name = "go_sdk")
        else:
            pv = _parse_version(version)
            if not pv:
                fail('go_register_toolchains: version must be a string like "1.15.5" or "host"')
            if _version_less(pv, MIN_SUPPORTED_VERSION):
                print("DEPRECATED: Go versions before {} are not supported and may not work".format(_version_string(MIN_SUPPORTED_VERSION)))
            go_download_sdk(
                name = "go_sdk",
                version = version,
            )

    if nogo:
        # Override default definition in go_rules_dependencies().
        go_register_nogo(
            name = "io_bazel_rules_nogo",
            nogo = nogo,
        )
