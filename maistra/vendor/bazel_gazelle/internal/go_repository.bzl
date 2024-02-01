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

load("//internal:common.bzl", "env_execute", "executable_extension")
load("@bazel_gazelle//internal:go_repository_cache.bzl", "read_cache_env")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "read_netrc", "use_netrc")

_DOC = """
`go_repository` downloads a Go project and generates build files with Gazelle
if they are not already present. This is the simplest way to depend on
external Go projects.

When `go_repository` is in module mode, it saves downloaded modules in a shared,
internal cache within Bazel's cache. It may be cleared with `bazel clean --expunge`.
By setting the environment variable `GO_REPOSITORY_USE_HOST_CACHE=1`, you can
force `go_repository` to use the module cache on the host system in the location
returned by `go env GOPATH`.

**Example**

```starlark
load("@bazel_gazelle//:deps.bzl", "go_repository")

# Download using "go mod download"
go_repository(
    name = "com_github_pkg_errors",
    importpath = "github.com/pkg/errors",
    sum = "h1:iURUrRGxPUNPdy5/HRSm+Yj6okJ6UtLINN0Q9M4+h3I=",
    version = "v0.8.1",
)

# Download automatically via git
go_repository(
    name = "com_github_pkg_errors",
    commit = "816c9085562cd7ee03e7f8188a1cfd942858cded",
    importpath = "github.com/pkg/errors",
)

# Download from git fork
go_repository(
    name = "com_github_pkg_errors",
    commit = "816c9085562cd7ee03e7f8188a1cfd942858cded",
    importpath = "github.com/pkg/errors",
    remote = "https://example.com/fork/github.com/pkg/errors",
    vcs = "git",
)

# Download via HTTP
go_repository(
    name = "com_github_pkg_errors",
    importpath = "github.com/pkg/errors",
    urls = ["https://codeload.github.com/pkg/errors/zip/816c9085562cd7ee03e7f8188a1cfd942858cded"],
    strip_prefix = "errors-816c9085562cd7ee03e7f8188a1cfd942858cded",
    type = "zip",
)
```

"""

# copied from
# https://github.com/bazelbuild/bazel/blob/d273cb62f43ef8169415cf60fc96e503ea2ad823/tools/build_defs/repo/http.bzl#L76
_AUTH_PATTERN_DOC = """An optional dict mapping host names to custom authorization patterns.

If a URL's host name is present in this dict the value will be used as a pattern when
generating the authorization header for the http request. This enables the use of custom
authorization schemes used in a lot of common cloud storage providers.

The pattern currently supports 2 tokens: <code>&lt;login&gt;</code> and
<code>&lt;password&gt;</code>, which are replaced with their equivalent value
in the netrc file for the same host name. After formatting, the result is set
as the value for the <code>Authorization</code> field of the HTTP request.

Example attribute and netrc for a http download to an oauth2 enabled API using a bearer token:

<pre>
auth_patterns = {
    "storage.cloudprovider.com": "Bearer &lt;password&gt;"
}
</pre>

netrc:

<pre>
machine storage.cloudprovider.com
        password RANDOM-TOKEN
</pre>

The final HTTP request would have the following header:

<pre>
Authorization: Bearer RANDOM-TOKEN
</pre>
"""

# We can't disable timeouts on Bazel, but we can set them to large values.
_GO_REPOSITORY_TIMEOUT = 86400

# Inspired by @bazel_tools//tools/build_defs/repo:http.bzl
def _get_auth(ctx, urls):
    """Given the list of URLs obtain the correct auth dict."""
    netrcfile = ""
    if "NETRC" in ctx.os.environ:
        netrcfile = ctx.os.environ["NETRC"]
    elif ctx.os.name.startswith("windows"):
        if "USERPROFILE" in ctx.os.environ:
            netrcfile = "%s/_netrc" % (ctx.os.environ["USERPROFILE"])
    elif "HOME" in ctx.os.environ:
        netrcfile = "%s/.netrc" % (ctx.os.environ["HOME"])

    if netrcfile and ctx.path(netrcfile).exists:
        netrc = read_netrc(ctx, netrcfile)
        return use_netrc(netrc, urls, ctx.attr.auth_patterns)

    return {}

def _go_repository_impl(ctx):
    # TODO(#549): vcs repositories are not cached and still need to be fetched.
    # Download the repository or module.
    fetch_repo_args = None
    gazelle_path = None

    # Declare Label dependencies at the top of function to avoid unnecessary fetching:
    # https://docs.bazel.build/versions/main/skylark/repository_rules.html#when-is-the-implementation-function-executed
    go_env_cache = str(ctx.path(Label("@bazel_gazelle_go_repository_cache//:go.env")))
    if not ctx.attr.urls:
        fetch_repo = str(ctx.path(Label("@bazel_gazelle_go_repository_tools//:bin/fetch_repo{}".format(executable_extension(ctx)))))
    generate = ctx.attr.build_file_generation == "on"
    _gazelle = "@bazel_gazelle_go_repository_tools//:bin/gazelle{}".format(executable_extension(ctx))
    if generate:
        gazelle_path = ctx.path(Label(_gazelle))

    if ctx.attr.urls:
        # HTTP mode
        for key in ("commit", "tag", "vcs", "remote", "version", "sum", "replace"):
            if getattr(ctx.attr, key):
                fail("cannot specifiy both urls and %s" % key, key)
        ctx.download_and_extract(
            url = ctx.attr.urls,
            sha256 = ctx.attr.sha256,
            canonical_id = ctx.attr.canonical_id,
            stripPrefix = ctx.attr.strip_prefix,
            type = ctx.attr.type,
            auth = _get_auth(ctx, ctx.attr.urls),
        )
    elif ctx.attr.commit or ctx.attr.tag:
        # repository mode
        if ctx.attr.commit:
            rev = ctx.attr.commit
            rev_key = "commit"
        elif ctx.attr.tag:
            rev = ctx.attr.tag
            rev_key = "tag"
        for key in ("urls", "strip_prefix", "type", "sha256", "version", "sum", "replace", "canonical_id"):
            if getattr(ctx.attr, key):
                fail("cannot specify both %s and %s" % (rev_key, key), key)

        if ctx.attr.vcs and not ctx.attr.remote:
            fail("if vcs is specified, remote must also be")

        fetch_repo_args = ["-dest", ctx.path(""), "-importpath", ctx.attr.importpath]
        if ctx.attr.remote:
            fetch_repo_args.extend(["--remote", ctx.attr.remote])
        if rev:
            fetch_repo_args.extend(["--rev", rev])
        if ctx.attr.vcs:
            fetch_repo_args.extend(["--vcs", ctx.attr.vcs])
    elif ctx.attr.version:
        # module mode
        for key in ("urls", "strip_prefix", "type", "sha256", "commit", "tag", "vcs", "remote"):
            if getattr(ctx.attr, key):
                fail("cannot specify both version and %s" % key)
        if not ctx.attr.sum:
            fail("if version is specified, sum must also be")

        fetch_path = ctx.attr.replace if ctx.attr.replace else ctx.attr.importpath
        fetch_repo_args = [
            "-dest=" + str(ctx.path("")),
            "-importpath=" + fetch_path,
            "-version=" + ctx.attr.version,
            "-sum=" + ctx.attr.sum,
        ]
    else:
        fail("one of urls, commit, tag, or importpath must be specified")

    env = read_cache_env(ctx, go_env_cache)
    env_keys = [
        # Respect user proxy and sumdb settings for privacy.
        # TODO(jayconrod): gazelle in go_repository mode should probably
        # not go out to the network at all. This means *the build*
        # goes out to the network. We tolerate this for downloading
        # archives, but finding module roots is a bit much.
        "GOPROXY",
        "GONOPROXY",
        "GOPRIVATE",
        "GOSUMDB",
        "GONOSUMDB",

        # PATH is needed to locate git and other vcs tools.
        "PATH",

        # HOME is needed to locate vcs configuration files (.gitconfig).
        "HOME",

        # Settings below are used by vcs tools.
        "SSH_AUTH_SOCK",
        "SSL_CERT_FILE",
        "SSL_CERT_DIR",
        "HTTP_PROXY",
        "HTTPS_PROXY",
        "NO_PROXY",
        "http_proxy",
        "https_proxy",
        "no_proxy",
        "GIT_SSL_CAINFO",
        "GIT_SSH",
        "GIT_SSH_COMMAND",
    ]
    env.update({k: ctx.os.environ[k] for k in env_keys if k in ctx.os.environ})

    if fetch_repo_args:
        # Disable sumdb in fetch_repo. In module mode, the sum is a mandatory
        # attribute of go_repository, so we don't need to look it up.
        fetch_repo_env = dict(env)
        fetch_repo_env["GOSUMDB"] = "off"

        # Override external GO111MODULE, because it is needed by module mode, no-op in repository mode
        fetch_repo_env["GO111MODULE"] = "on"

        result = env_execute(
            ctx,
            [fetch_repo] + fetch_repo_args,
            environment = fetch_repo_env,
            timeout = _GO_REPOSITORY_TIMEOUT,
        )
        if result.return_code:
            fail("failed to fetch %s: %s" % (ctx.name, result.stderr))
        if ctx.attr.debug_mode and result.stderr:
            print("fetch_repo: " + result.stderr)

    # Repositories are fetched. Determine if build file generation is needed.
    build_file_names = ctx.attr.build_file_name.split(",")
    existing_build_file = ""
    for name in build_file_names:
        path = ctx.path(name)
        if path.exists and not env_execute(ctx, ["test", "-f", path]).return_code:
            existing_build_file = name
            break

    generate = generate or (not existing_build_file and ctx.attr.build_file_generation == "auto")

    if generate:
        # Build file generation is needed. Populate Gazelle directive at root build file
        build_file_name = existing_build_file or build_file_names[0]
        if len(ctx.attr.build_directives) > 0:
            ctx.file(
                build_file_name,
                "\n".join(["# " + d for d in ctx.attr.build_directives]),
            )

        # Run Gazelle
        if gazelle_path == None:
            gazelle_path = ctx.path(Label(_gazelle))
        cmd = [
            gazelle_path,
            "-go_repository_mode",
            "-go_prefix",
            ctx.attr.importpath,
            "-mode",
            "fix",
            "-repo_root",
            ctx.path(""),
            "-repo_config",
            ctx.path(ctx.attr.build_config),
        ]
        if ctx.attr.version:
            cmd.append("-go_repository_module_mode")
        if ctx.attr.build_file_name:
            cmd.extend(["-build_file_name", ctx.attr.build_file_name])
        if ctx.attr.build_tags:
            cmd.extend(["-build_tags", ",".join(ctx.attr.build_tags)])
        if ctx.attr.build_external:
            cmd.extend(["-external", ctx.attr.build_external])
        if ctx.attr.build_file_proto_mode:
            cmd.extend(["-proto", ctx.attr.build_file_proto_mode])
        if ctx.attr.build_naming_convention:
            cmd.extend(["-go_naming_convention", ctx.attr.build_naming_convention])
        cmd.extend(ctx.attr.build_extra_args)
        cmd.append(ctx.path(""))
        ctx.report_progress("running Gazelle")
        result = env_execute(ctx, cmd, environment = env, timeout = _GO_REPOSITORY_TIMEOUT)
        if result.return_code:
            fail("failed to generate BUILD files for %s: %s" % (
                ctx.attr.importpath,
                result.stderr,
            ))
        if ctx.attr.debug_mode and result.stderr:
            print("%s: %s" % (ctx.name, result.stderr))

    # Apply patches if necessary.
    patch(ctx)

go_repository = repository_rule(
    implementation = _go_repository_impl,
    doc = _DOC,
    attrs = {
        # Fundamental attributes of a go repository
        "importpath": attr.string(
            doc = """The Go import path that matches the root directory of this repository.

            In module mode (when `version` is set), this must be the module path. If
            neither `urls` nor `remote` is specified, `go_repository` will
            automatically find the true path of the module, applying import path
            redirection.

            If build files are generated for this repository, libraries will have their
            `importpath` attributes prefixed with this `importpath` string.  """,
            mandatory = True,
        ),

        # Attributes for a repository that should be checked out from VCS
        "commit": attr.string(
            doc = """If the repository is downloaded using a version control tool, this is the
            commit or revision to check out. With git, this would be a sha1 commit id.
            `commit` and `tag` may not both be set.""",
        ),
        "tag": attr.string(
            doc = """If the repository is downloaded using a version control tool, this is the
            named revision to check out. `commit` and `tag` may not both be set.""",
        ),
        "vcs": attr.string(
            default = "",
            doc = """One of `"git"`, `"hg"`, `"svn"`, `"bzr"`.

            The version control system to use. This is usually determined automatically,
            but it may be necessary to set this when `remote` is set and the VCS cannot
            be inferred. You must have the corresponding tool installed on your host.""",
            values = [
                "",
                "git",
                "hg",
                "svn",
                "bzr",
            ],
        ),
        "remote": attr.string(
            doc = """The VCS location where the repository should be downloaded from. This is
            usually inferred from `importpath`, but you can set `remote` to download
            from a private repository or a fork.""",
        ),

        # Attributes for a repository that should be downloaded via HTTP.
        "urls": attr.string_list(
            doc = """A list of HTTP(S) URLs where an archive containing the project can be
            downloaded. Bazel will attempt to download from the first URL; the others
            are mirrors.""",
        ),
        "strip_prefix": attr.string(
            doc = """If the repository is downloaded via HTTP (`urls` is set), this is a
            directory prefix to strip. See [`http_archive.strip_prefix`].""",
        ),
        "type": attr.string(
            doc = """One of `"zip"`, `"tar.gz"`, `"tgz"`, `"tar.bz2"`, `"tar.xz"`.

            If the repository is downloaded via HTTP (`urls` is set), this is the
            file format of the repository archive. This is normally inferred from the
            downloaded file name.""",
        ),
        "sha256": attr.string(
            doc = """If the repository is downloaded via HTTP (`urls` is set), this is the
            SHA-256 sum of the downloaded archive. When set, Bazel will verify the archive
            against this sum before extracting it.

            **CAUTION:** Do not use this with services that prepare source archives on
            demand, such as codeload.github.com. Any minor change in the server software
            can cause differences in file order, alignment, and compression that break
            SHA-256 sums.""",
        ),
        "canonical_id": attr.string(
            doc = """If the repository is downloaded via HTTP (`urls` is set) and this is set, restrict cache hits to those cases where the
            repository was added to the cache with the same canonical id.""",
        ),
        "auth_patterns": attr.string_dict(
            doc = _AUTH_PATTERN_DOC,
        ),

        # Attributes for a module that should be downloaded with the Go toolchain.
        "version": attr.string(
            doc = """If specified, `go_repository` will download the module at this version
            using `go mod download`. `sum` must also be set. `commit`, `tag`,
            and `urls` may not be set. """,
        ),
        "sum": attr.string(
            doc = """A hash of the module contents. In module mode, `go_repository` will verify
            the downloaded module matches this sum. May only be set when `version`
            is also set.

            A value for `sum` may be found in the `go.sum` file or by running
            `go mod download -json <module>@<version>`.""",
        ),
        "replace": attr.string(
            doc = """A replacement for the module named by `importpath`. The module named by
            `replace` will be downloaded at `version` and verified with `sum`.

            NOTE: There is no `go_repository` equivalent to file path `replace`
            directives. Use `local_repository` instead.""",
        ),

        # Attributes for a repository that needs automatic build file generation
        "build_external": attr.string(
            default = "static",
            doc = """One of `"external"`, `"static"` or `"vendored"`.

            This sets Gazelle's `-external` command line flag. In `"static"` mode,
            Gazelle will not call out to the network to resolve imports.

            **NOTE:** This cannot be used to ignore the `vendor` directory in a
            repository. The `-external` flag only controls how Gazelle resolves
            imports which are not present in the repository. Use
            `build_extra_args = ["-exclude=vendor"]` instead.""",
            values = [
                "",
                "external",
                "static",
                "vendored",
            ],
        ),
        "build_file_name": attr.string(
            default = "BUILD.bazel,BUILD",
            doc = """Comma-separated list of names Gazelle will consider to be build files.
            If a repository contains files named `build` that aren't related to Bazel,
            it may help to set this to `"BUILD.bazel"`, especially on case-insensitive
            file systems.""",
        ),
        "build_file_generation": attr.string(
            default = "auto",
            doc = """One of `"auto"`, `"on"`, `"off"`.

            Whether Gazelle should generate build files in the repository. In `"auto"`
            mode, Gazelle will run if there is no build file in the repository root
            directory.""",
            values = [
                "on",
                "auto",
                "off",
            ],
        ),
        "build_naming_convention": attr.string(
            values = [
                "go_default_library",
                "import",
                "import_alias",
            ],
            default = "import_alias",
            doc = """Sets the library naming convention to use when resolving dependencies against this external
            repository. If unset, the convention from the external workspace is used.
            Legal values are `go_default_library`, `import`, and `import_alias`.

            See the `gazelle:go_naming_convention` directive in [Directives] for more information.""",
        ),
        "build_tags": attr.string_list(
            doc = "This sets Gazelle's `-build_tags` command line flag.",
        ),
        "build_file_proto_mode": attr.string(
            doc = """One of `"default"`, `"legacy"`, `"disable"`, `"disable_global"` or `"package"`.

            This sets Gazelle's `-proto` command line flag. See Directives_ for more
            information on each mode.""",
            values = [
                "",
                "default",
                "package",
                "disable",
                "disable_global",
                "legacy",
            ],
        ),
        "build_extra_args": attr.string_list(
            doc = "A list of additional command line arguments to pass to Gazelle when generating build files.",
        ),
        "build_config": attr.label(
            default = "@bazel_gazelle_go_repository_config//:WORKSPACE",
            doc = """A file that Gazelle should read to learn about external repositories before
            generating build files. This is useful for dependency resolution. For example,
            a `go_repository` rule in this file establishes a mapping between a
            repository name like `golang.org/x/tools` and a workspace name like
            `org_golang_x_tools`. Workspace directives like
            `# gazelle:repository_macro` are recognized.

            `go_repository` rules will be re-evaluated when parts of WORKSPACE related
            to Gazelle's configuration are changed, including Gazelle directives and
            `go_repository` `name` and `importpath` attributes.
            Their content should still be fetched from a local cache, but build files
            will be regenerated. If this is not desirable, `build_config` may be set
            to a less frequently updated file or `None` to disable this functionality.""",
        ),
        "build_directives": attr.string_list(
            default = [],
            doc = """A list of directives to be written to the root level build file before
            Calling Gazelle to generate build files. Each string in the list will be
            prefixed with `#` automatically. A common use case is to pass a list of
            Gazelle directives.""",
        ),

        # Patches to apply after running gazelle.
        "patches": attr.label_list(
            doc = "A list of patches to apply to the repository after gazelle runs.",
        ),
        "patch_tool": attr.string(
            default = "patch",
            doc = "The patch tool used to apply `patches`.",
        ),
        "patch_args": attr.string_list(
            default = ["-p0"],
            doc = "Arguments passed to the patch tool when applying patches.",
        ),
        "patch_cmds": attr.string_list(
            default = [],
            doc = "Commands to run in the repository after patches are applied.",
        ),

        # Attributes that affect the verbosity of logging:
        "debug_mode": attr.bool(
            default = False,
            doc = """Enables logging of fetch_repo and Gazelle output during succcesful runs. Gazelle can be noisy
            so this defaults to `False`. However, setting to `True` can be useful for debugging build failures and
            unexpected behavior for the given rule.
            """,
        ),
    },
)
"""See repository.md#go-repository for full documentation."""

# Copied from @bazel_tools//tools/build_defs/repo:utils.bzl
#
# Here we shell out to `patch` executable instead of calling repository_ctx.patch()
# so that we can apply the patch files _AFTER_ gazelle ran and generated the
# BUILD files.  This enables patch files to modify the generated BUILD files.
def patch(ctx):
    """Implementation of patching an already extracted repository"""
    bash_exe = ctx.os.environ["BAZEL_SH"] if "BAZEL_SH" in ctx.os.environ else "bash"
    for patchfile in ctx.attr.patches:
        command = "{patchtool} {patch_args} < {patchfile}".format(
            patchtool = ctx.attr.patch_tool,
            patchfile = ctx.path(patchfile),
            patch_args = " ".join([
                "'%s'" % arg
                for arg in ctx.attr.patch_args
            ]),
        )
        st = ctx.execute([bash_exe, "-c", command])
        if st.return_code:
            fail("Error applying patch %s:\n%s%s" %
                 (str(patchfile), st.stderr, st.stdout))
    for cmd in ctx.attr.patch_cmds:
        st = ctx.execute([bash_exe, "-c", cmd])
        if st.return_code:
            fail("Error applying patch command %s:\n%s%s" %
                 (cmd, st.stdout, st.stderr))
