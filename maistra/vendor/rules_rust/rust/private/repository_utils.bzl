"""Utility macros for use in rules_rust repository rules"""

load("//rust:known_shas.bzl", "FILE_KEY_TO_SHA")
load(
    "//rust/platform:triple_mappings.bzl",
    "system_to_binary_ext",
    "system_to_dylib_ext",
    "system_to_staticlib_ext",
    "system_to_stdlib_linkflags",
)

DEFAULT_TOOLCHAIN_NAME_PREFIX = "toolchain_for"
DEFAULT_STATIC_RUST_URL_TEMPLATES = ["https://static.rust-lang.org/dist/{}.tar.gz"]

_build_file_for_compiler_template = """\
filegroup(
    name = "rustc",
    srcs = ["bin/rustc{binary_ext}"],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "rustc_lib",
    srcs = glob(
        [
            "bin/*{dylib_ext}",
            "lib/*{dylib_ext}",
            "lib/rustlib/{target_triple}/codegen-backends/*{dylib_ext}",
            "lib/rustlib/{target_triple}/bin/rust-lld{binary_ext}",
            "lib/rustlib/{target_triple}/lib/*{dylib_ext}",
        ],
        allow_empty = True,
    ),
    visibility = ["//visibility:public"],
)

filegroup(
    name = "rustdoc",
    srcs = ["bin/rustdoc{binary_ext}"],
    visibility = ["//visibility:public"],
)
"""

def BUILD_for_compiler(target_triple):
    """Emits a BUILD file the compiler archive.

    Args:
        target_triple (str): The triple of the target platform

    Returns:
        str: The contents of a BUILD file
    """
    return _build_file_for_compiler_template.format(
        binary_ext = system_to_binary_ext(target_triple.system),
        staticlib_ext = system_to_staticlib_ext(target_triple.system),
        dylib_ext = system_to_dylib_ext(target_triple.system),
        target_triple = target_triple.str,
    )

_build_file_for_cargo_template = """\
filegroup(
    name = "cargo",
    srcs = ["bin/cargo{binary_ext}"],
    visibility = ["//visibility:public"],
)"""

def BUILD_for_cargo(target_triple):
    """Emits a BUILD file the cargo archive.

    Args:
        target_triple (str): The triple of the target platform

    Returns:
        str: The contents of a BUILD file
    """
    return _build_file_for_cargo_template.format(
        binary_ext = system_to_binary_ext(target_triple.system),
    )

_build_file_for_rustfmt_template = """\
filegroup(
    name = "rustfmt_bin",
    srcs = ["bin/rustfmt{binary_ext}"],
    visibility = ["//visibility:public"],
)

sh_binary(
    name = "rustfmt",
    srcs = [":rustfmt_bin"],
    visibility = ["//visibility:public"],
)
"""

def BUILD_for_rustfmt(target_triple):
    """Emits a BUILD file the rustfmt archive.

    Args:
        target_triple (str): The triple of the target platform

    Returns:
        str: The contents of a BUILD file
    """
    return _build_file_for_rustfmt_template.format(
        binary_ext = system_to_binary_ext(target_triple.system),
    )

_build_file_for_clippy_template = """\
filegroup(
    name = "clippy_driver_bin",
    srcs = ["bin/clippy-driver{binary_ext}"],
    visibility = ["//visibility:public"],
)
"""

_build_file_for_rust_analyzer_proc_macro_srv = """\
filegroup(
   name = "rust_analyzer_proc_macro_srv",
   srcs = ["libexec/rust-analyzer-proc-macro-srv{binary_ext}"],
   visibility = ["//visibility:public"],
)
"""

def BUILD_for_rust_analyzer_proc_macro_srv(exec_triple):
    """Emits a BUILD file the rust_analyzer_proc_macro_srv archive.

    Args:
        exec_triple (str): The triple of the exec platform
    Returns:
        str: The contents of a BUILD file
    """
    return _build_file_for_rust_analyzer_proc_macro_srv.format(
        binary_ext = system_to_binary_ext(exec_triple.system),
    )

def BUILD_for_clippy(target_triple):
    """Emits a BUILD file the clippy archive.

    Args:
        target_triple (str): The triple of the target platform

    Returns:
        str: The contents of a BUILD file
    """
    return _build_file_for_clippy_template.format(
        binary_ext = system_to_binary_ext(target_triple.system),
    )

_build_file_for_llvm_tools = """\
filegroup(
    name = "llvm_cov_bin",
    srcs = ["lib/rustlib/{target_triple}/bin/llvm-cov{binary_ext}"],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "llvm_profdata_bin",
    srcs = ["lib/rustlib/{target_triple}/bin/llvm-profdata{binary_ext}"],
    visibility = ["//visibility:public"],
)
"""

def BUILD_for_llvm_tools(target_triple):
    """Emits a BUILD file the llvm-tools binaries.

    Args:
        target_triple (struct): The triple of the target platform

    Returns:
        str: The contents of a BUILD file
    """
    return _build_file_for_llvm_tools.format(
        binary_ext = system_to_binary_ext(target_triple.system),
        target_triple = target_triple.str,
    )

_build_file_for_stdlib_template = """\
load("@rules_rust//rust:toolchain.bzl", "rust_stdlib_filegroup")

rust_stdlib_filegroup(
    name = "rust_std-{target_triple}",
    srcs = glob(
        [
            "lib/rustlib/{target_triple}/lib/*.rlib",
            "lib/rustlib/{target_triple}/lib/*{dylib_ext}",
            "lib/rustlib/{target_triple}/lib/*{staticlib_ext}",
            "lib/rustlib/{target_triple}/lib/self-contained/**",
        ],
        # Some patterns (e.g. `lib/*.a`) don't match anything, see https://github.com/bazelbuild/rules_rust/pull/245
        allow_empty = True,
    ),
    visibility = ["//visibility:public"],
)

# For legacy support
alias(
    name = "rust_lib-{target_triple}",
    actual = "rust_std-{target_triple}",
    visibility = ["//visibility:public"],
)
"""

def BUILD_for_stdlib(target_triple):
    """Emits a BUILD file the stdlib archive.

    Args:
        target_triple (triple): The triple of the target platform

    Returns:
        str: The contents of a BUILD file
    """
    return _build_file_for_stdlib_template.format(
        binary_ext = system_to_binary_ext(target_triple.system),
        staticlib_ext = system_to_staticlib_ext(target_triple.system),
        dylib_ext = system_to_dylib_ext(target_triple.system),
        target_triple = target_triple.str,
    )

_build_file_for_rust_toolchain_template = """\
load("@rules_rust//rust:toolchain.bzl", "rust_toolchain")

rust_toolchain(
    name = "{toolchain_name}",
    rust_doc = "@{workspace_name}//:rustdoc",
    rust_std = "@{workspace_name}//:rust_std-{target_triple}",
    rustc = "@{workspace_name}//:rustc",
    rustfmt = {rustfmt_label},
    cargo = "@{workspace_name}//:cargo",
    clippy_driver = "@{workspace_name}//:clippy_driver_bin",
    llvm_cov = {llvm_cov_label},
    llvm_profdata = {llvm_profdata_label},
    rustc_lib = "@{workspace_name}//:rustc_lib",
    allocator_library = {allocator_library},
    binary_ext = "{binary_ext}",
    staticlib_ext = "{staticlib_ext}",
    dylib_ext = "{dylib_ext}",
    stdlib_linkflags = [{stdlib_linkflags}],
    os = "{system}",
    default_edition = "{default_edition}",
    exec_triple = "{exec_triple}",
    target_triple = "{target_triple}",
    visibility = ["//visibility:public"],
)
"""

def BUILD_for_rust_toolchain(
        workspace_name,
        name,
        exec_triple,
        target_triple,
        allocator_library,
        default_edition,
        include_rustfmt,
        include_llvm_tools,
        stdlib_linkflags = None):
    """Emits a toolchain declaration to match an existing compiler and stdlib.

    Args:
        workspace_name (str): The name of the workspace that this toolchain resides in
        name (str): The name of the toolchain declaration
        exec_triple (triple): The rust-style target that this compiler runs on
        target_triple (triple): The rust-style target triple of the tool
        allocator_library (str, optional): Target that provides allocator functions when rust_library targets are embedded in a cc_binary.
        default_edition (str): Default Rust edition.
        include_rustfmt (bool): Whether rustfmt is present in the toolchain.
        include_llvm_tools (bool): Whether llvm-tools are present in the toolchain.
        stdlib_linkflags (list, optional): Overriden flags needed for linking to rust
                                           stdlib, akin to BAZEL_LINKLIBS. Defaults to
                                           None.


    Returns:
        str: A rendered template of a `rust_toolchain` declaration
    """
    if stdlib_linkflags == None:
        stdlib_linkflags = ", ".join(['"%s"' % x for x in system_to_stdlib_linkflags(target_triple.system)])

    rustfmt_label = "None"
    if include_rustfmt:
        rustfmt_label = "\"@{workspace_name}//:rustfmt_bin\"".format(workspace_name = workspace_name)
    llvm_cov_label = "None"
    llvm_profdata_label = "None"
    if include_llvm_tools:
        llvm_cov_label = "\"@{workspace_name}//:llvm_cov_bin\"".format(workspace_name = workspace_name)
        llvm_profdata_label = "\"@{workspace_name}//:llvm_profdata_bin\"".format(workspace_name = workspace_name)
    allocator_library_label = "None"
    if allocator_library:
        allocator_library_label = "\"{allocator_library}\"".format(allocator_library = allocator_library)

    return _build_file_for_rust_toolchain_template.format(
        toolchain_name = name,
        workspace_name = workspace_name,
        binary_ext = system_to_binary_ext(target_triple.system),
        staticlib_ext = system_to_staticlib_ext(target_triple.system),
        dylib_ext = system_to_dylib_ext(target_triple.system),
        allocator_library = allocator_library_label,
        stdlib_linkflags = stdlib_linkflags,
        system = target_triple.system,
        default_edition = default_edition,
        exec_triple = exec_triple.str,
        target_triple = target_triple.str,
        rustfmt_label = rustfmt_label,
        llvm_cov_label = llvm_cov_label,
        llvm_profdata_label = llvm_profdata_label,
    )

_build_file_for_toolchain_template = """\
toolchain(
    name = "{name}",
    exec_compatible_with = {exec_constraint_sets_serialized},
    target_compatible_with = {target_constraint_sets_serialized},
    toolchain = "{toolchain}",
    toolchain_type = "{toolchain_type}",
    {target_settings}
)
"""

def BUILD_for_toolchain(
        name,
        toolchain,
        toolchain_type,
        target_settings,
        target_compatible_with,
        exec_compatible_with):
    target_settings_value = "target_settings = {},".format(json.encode(target_settings)) if target_settings else "# target_settings = []"

    return _build_file_for_toolchain_template.format(
        name = name,
        exec_constraint_sets_serialized = json.encode(exec_compatible_with),
        target_constraint_sets_serialized = json.encode(target_compatible_with),
        toolchain = toolchain,
        toolchain_type = toolchain_type,
        target_settings = target_settings_value,
    )

def load_rustfmt(ctx, target_triple, version, iso_date):
    """Loads a rustfmt binary and yields corresponding BUILD for it

    Args:
        ctx (repository_ctx): The repository rule's context object.
        target_triple (struct): The platform triple to download rustfmt for.
        version (str): The version or channel of rustfmt.
        iso_date (str): The date of the tool (or None, if the version is a specific version).

    Returns:
        str: The BUILD file contents for this rustfmt binary
    """

    load_arbitrary_tool(
        ctx,
        iso_date = iso_date,
        target_triple = target_triple,
        tool_name = "rustfmt",
        tool_subdirectories = ["rustfmt-preview"],
        version = version,
    )

    return BUILD_for_rustfmt(target_triple)

def load_rust_compiler(ctx, iso_date, target_triple, version):
    """Loads a rust compiler and yields corresponding BUILD for it

    Args:
        ctx (repository_ctx): A repository_ctx.
        iso_date (str): The date of the tool (or None, if the version is a specific version).
        target_triple (struct): The Rust-style target that this compiler runs on.
        version (str): The version of the tool among \"nightly\", \"beta\", or an exact version.

    Returns:
        str: The BUILD file contents for this compiler and compiler library
    """

    load_arbitrary_tool(
        ctx,
        iso_date = iso_date,
        target_triple = target_triple,
        tool_name = "rustc",
        tool_subdirectories = ["rustc"],
        version = version,
    )

    return BUILD_for_compiler(target_triple)

def load_clippy(ctx, iso_date, target_triple, version):
    """Loads Clippy and yields corresponding BUILD for it

    Args:
        ctx (repository_ctx): A repository_ctx.
        iso_date (str): The date of the tool (or None, if the version is a specific version).
        target_triple (struct): The Rust-style target that this compiler runs on.
        version (str): The version of the tool among \"nightly\", \"beta\", or an exact version.

    Returns:
        str: The BUILD file contents for Clippy
    """
    load_arbitrary_tool(
        ctx,
        iso_date = iso_date,
        target_triple = target_triple,
        tool_name = "clippy",
        tool_subdirectories = ["clippy-preview"],
        version = version,
    )

    return BUILD_for_clippy(target_triple)

def load_cargo(ctx, iso_date, target_triple, version):
    """Loads Cargo and yields corresponding BUILD for it

    Args:
        ctx (repository_ctx): A repository_ctx.
        iso_date (str): The date of the tool (or None, if the version is a specific version).
        target_triple (struct): The Rust-style target that this compiler runs on.
        version (str): The version of the tool among \"nightly\", \"beta\", or an exact version.

    Returns:
        str: The BUILD file contents for Cargo
    """

    load_arbitrary_tool(
        ctx,
        iso_date = iso_date,
        target_triple = target_triple,
        tool_name = "cargo",
        tool_subdirectories = ["cargo"],
        version = version,
    )

    return BUILD_for_cargo(target_triple)

def includes_rust_analyzer_proc_macro_srv(version, iso_date):
    """Determine whether or not the rust_analyzer_proc_macro_srv binary in available in the given version of Rust.

    Args:
        version (str): The version of the tool among \"nightly\", \"beta\", or an exact version.
        iso_date (str): The date of the tool (or None, if the version is a specific version).

    Returns:
        bool: Whether or not the binary is expected to be included
    """

    if version == "nightly":
        return iso_date >= "2022-09-21"
    elif version == "beta":
        return False
    elif version >= "1.64.0":
        return True

    return False

def load_rust_src(ctx, iso_date, version, sha256 = ""):
    """Loads the rust source code. Used by the rust-analyzer rust-project.json generator.

    Args:
        ctx (ctx): A repository_ctx.
        version (str): The version of the tool among "nightly", "beta', or an exact version.
        iso_date (str): The date of the tool (or None, if the version is a specific version).
        sha256 (str): The sha256 value for the `rust-src` artifact
    """
    tool_suburl = produce_tool_suburl("rust-src", None, version, iso_date)
    url = ctx.attr.urls[0].format(tool_suburl)

    tool_path = produce_tool_path("rust-src", version, None)
    archive_path = tool_path + _get_tool_extension(getattr(ctx.attr, "urls", None))
    sha256 = sha256 or getattr(ctx.attr, "sha256s", {}).get(archive_path) or FILE_KEY_TO_SHA.get(archive_path) or ""
    ctx.download_and_extract(
        url,
        output = "lib/rustlib/src",
        sha256 = sha256,
        auth = _make_auth_dict(ctx, [url]),
        stripPrefix = "{}/rust-src/lib/rustlib/src/rust".format(tool_path),
    )
    ctx.file(
        "lib/rustlib/src/BUILD.bazel",
        """\
filegroup(
    name = "rustc_srcs",
    srcs = glob(["**/*"]),
    visibility = ["//visibility:public"],
)""",
    )

_build_file_for_rust_analyzer_toolchain_template = """\
load("@rules_rust//rust:toolchain.bzl", "rust_analyzer_toolchain")

rust_analyzer_toolchain(
    name = "{name}",
    proc_macro_srv = {proc_macro_srv},
    rustc = "{rustc}",
    rustc_srcs = "//lib/rustlib/src:rustc_srcs",
    visibility = ["//visibility:public"],
)
"""

def BUILD_for_rust_analyzer_toolchain(name, rustc, proc_macro_srv):
    return _build_file_for_rust_analyzer_toolchain_template.format(
        name = name,
        rustc = rustc,
        proc_macro_srv = repr(proc_macro_srv),
    )

_build_file_for_rustfmt_toolchain_template = """\
load("@rules_rust//rust:toolchain.bzl", "rustfmt_toolchain")

rustfmt_toolchain(
    name = "{name}",
    rustfmt = "{rustfmt}",
    rustc = "{rustc}",
    rustc_lib = "{rustc_lib}",
    visibility = ["//visibility:public"],
)
"""

def BUILD_for_rustfmt_toolchain(name, rustfmt, rustc, rustc_lib):
    return _build_file_for_rustfmt_toolchain_template.format(
        name = name,
        rustfmt = rustfmt,
        rustc = rustc,
        rustc_lib = rustc_lib,
    )

def load_rust_stdlib(ctx, target_triple):
    """Loads a rust standard library and yields corresponding BUILD for it

    Args:
        ctx (repository_ctx): A repository_ctx.
        target_triple (struct): The rust-style target triple of the tool

    Returns:
        str: The BUILD file contents for this stdlib
    """

    load_arbitrary_tool(
        ctx,
        iso_date = ctx.attr.iso_date,
        target_triple = target_triple,
        tool_name = "rust-std",
        tool_subdirectories = ["rust-std-{}".format(target_triple.str)],
        version = ctx.attr.version,
    )

    return BUILD_for_stdlib(target_triple)

def load_rustc_dev_nightly(ctx, target_triple):
    """Loads the nightly rustc dev component

    Args:
        ctx: A repository_ctx.
        target_triple: The rust-style target triple of the tool
    """

    subdir_name = "rustc-dev"
    if ctx.attr.iso_date < "2020-12-24":
        subdir_name = "rustc-dev-{}".format(target_triple)

    load_arbitrary_tool(
        ctx,
        iso_date = ctx.attr.iso_date,
        target_triple = target_triple,
        tool_name = "rustc-dev",
        tool_subdirectories = [subdir_name],
        version = ctx.attr.version,
    )

def load_llvm_tools(ctx, target_triple):
    """Loads the llvm tools

    Args:
        ctx: A repository_ctx.
        target_triple: The rust-style target triple of the tool
    """
    load_arbitrary_tool(
        ctx,
        iso_date = ctx.attr.iso_date,
        target_triple = target_triple,
        tool_name = "llvm-tools",
        tool_subdirectories = ["llvm-tools-preview"],
        version = ctx.attr.version,
    )

    return BUILD_for_llvm_tools(target_triple)

def check_version_valid(version, iso_date, param_prefix = ""):
    """Verifies that the provided rust version and iso_date make sense.

    Args:
        version (str): The rustc version
        iso_date (str): The rustc nightly version's iso date
        param_prefix (str, optional): The name of the tool who's version is being checked.
    """

    if not version and iso_date:
        fail("{param_prefix}iso_date must be paired with a {param_prefix}version".format(param_prefix = param_prefix))

    if version in ("beta", "nightly") and not iso_date:
        fail("{param_prefix}iso_date must be specified if version is 'beta' or 'nightly'".format(param_prefix = param_prefix))

def produce_tool_suburl(tool_name, target_triple, version, iso_date = None):
    """Produces a fully qualified Rust tool name for URL

    Args:
        tool_name (str): The name of the tool per `static.rust-lang.org`.
        target_triple (struct): The rust-style target triple of the tool.
        version (str): The version of the tool among "nightly", "beta', or an exact version.
        iso_date (str): The date of the tool (or None, if the version is a specific version).

    Returns:
        str: The fully qualified url path for the specified tool.
    """
    path = produce_tool_path(tool_name, version, target_triple)
    return iso_date + "/" + path if (iso_date and version in ("beta", "nightly")) else path

def produce_tool_path(tool_name, version, target_triple = None):
    """Produces a qualified Rust tool name

    Args:
        tool_name (str): The name of the tool per static.rust-lang.org
        version (str): The version of the tool among "nightly", "beta', or an exact version.
        target_triple (struct, optional): The rust-style target triple of the tool

    Returns:
        str: The qualified path for the specified tool.
    """
    if not tool_name:
        fail("No tool name was provided")
    if not version:
        fail("No tool version was provided")

    # Not all tools require a triple. E.g. `rustc_src` (Rust source files for rust-analyzer).
    platform_triple = None
    if target_triple:
        platform_triple = target_triple.str

    return "-".join([e for e in [tool_name, version, platform_triple] if e])

def lookup_tool_sha256(ctx, tool_name, target_triple, version, iso_date, sha256):
    """Looks up the sha256 hash of a specific tool archive.

    The lookup order is:

    1. The sha256s dict in the context attributes;
    2. The list of sha256 hashes populated in //rust:known_shas.bzl;
    3. The sha256 argument to the function

    Args:
        ctx (repository_ctx): A repository_ctx (no attrs required).
        tool_name (str): The name of the given tool per the archive naming.
        target_triple (struct): The rust-style target triple of the tool.
        version (str): The version of the tool among "nightly", "beta', or an exact version.
        iso_date (str): The date of the tool (ignored if the version is a specific version).
        sha256 (str): The expected hash of hash of the Rust tool.

    Returns:
        str: The sha256 of the tool archive, or an empty string if the hash could not be found.
    """
    tool_suburl = produce_tool_suburl(tool_name, target_triple, version, iso_date)
    archive_path = tool_suburl + _get_tool_extension(getattr(ctx.attr, "urls", None))
    return getattr(ctx.attr, "sha256s", dict()).get(archive_path) or FILE_KEY_TO_SHA.get(archive_path) or sha256

def load_arbitrary_tool(ctx, tool_name, tool_subdirectories, version, iso_date, target_triple, sha256 = ""):
    """Loads a Rust tool, downloads, and extracts into the common workspace.

    This function sources the tool from the Rust-lang static file server. The index is available at:
    - https://static.rust-lang.org/dist/channel-rust-stable.toml
    - https://static.rust-lang.org/dist/channel-rust-beta.toml
    - https://static.rust-lang.org/dist/channel-rust-nightly.toml

    Args:
        ctx (repository_ctx): A repository_ctx (no attrs required).
        tool_name (str): The name of the given tool per the archive naming.
        tool_subdirectories (str): The subdirectories of the tool files (at a level below the root directory of
            the archive). The root directory of the archive is expected to match
            $TOOL_NAME-$VERSION-$TARGET_TRIPLE.
            Example:
            tool_name
            |    version
            |    |      target_triple
            v    v      v
            rust-1.39.0-x86_64-unknown-linux-gnu/clippy-preview
                                             .../rustc
                                             .../etc
            tool_subdirectories = ["clippy-preview", "rustc"]
        version (str): The version of the tool among "nightly", "beta', or an exact version.
        iso_date (str): The date of the tool (ignored if the version is a specific version).
        target_triple (struct): The rust-style target triple of the tool.
        sha256 (str, optional): The expected hash of hash of the Rust tool. Defaults to "".
    """
    check_version_valid(version, iso_date, param_prefix = tool_name + "_")

    # View the indices mentioned in the docstring to find the tool_suburl for a given
    # tool.
    tool_suburl = produce_tool_suburl(tool_name, target_triple, version, iso_date)
    urls = []

    for url in getattr(ctx.attr, "urls", DEFAULT_STATIC_RUST_URL_TEMPLATES):
        new_url = url.format(tool_suburl)
        if new_url not in urls:
            urls.append(new_url)

    tool_path = produce_tool_path(tool_name, version, target_triple)

    sha256 = lookup_tool_sha256(ctx, tool_name, target_triple, version, iso_date, sha256)

    for subdirectory in tool_subdirectories:
        # As long as the sha256 value is consistent accross calls here the
        # cost of downloading an artifact is negated as by Bazel's caching.
        result = ctx.download_and_extract(
            urls,
            sha256 = sha256,
            auth = _make_auth_dict(ctx, urls),
            stripPrefix = "{}/{}".format(tool_path, subdirectory),
        )

        # In the event no sha256 was provided, set it to the value of the first
        # downloaded item so subsequent downloads use a cached artifact.
        if not sha256:
            sha256 = result.sha256

def _make_auth_dict(ctx, urls):
    auth = getattr(ctx.attr, "auth", {})
    if not auth:
        return {}
    ret = {}
    for url in urls:
        ret[url] = auth
    return ret

def _get_tool_extension(urls = None):
    if urls == None:
        urls = DEFAULT_STATIC_RUST_URL_TEMPLATES
    if urls[0][-7:] == ".tar.gz":
        return ".tar.gz"
    elif urls[0][-7:] == ".tar.xz":
        return ".tar.xz"
    else:
        return ""

def select_rust_version(versions):
    """Select the highest priorty version for a list of Rust versions

    Priority order: `stable > nightly > beta`

    Note that duplicate channels are unexpected in `versions`.

    Args:
        versions (list): A list of Rust versions. E.g. [`1.66.0`, `nightly/2022-12-15`]

    Returns:
        str: The highest ranking value from `versions`
    """
    if not versions:
        fail("No versions were provided")

    current = versions[0]

    for ver in versions:
        if ver.startswith("beta"):
            if current[0].isdigit() or current.startswith("nightly"):
                continue
            if current.startswith("beta") and ver > current:
                current = ver
                continue

            current = ver
        elif ver.startswith("nightly"):
            if current[0].isdigit():
                continue
            if current.startswith("nightly") and ver > current:
                current = ver
                continue

            current = ver

        else:
            current = ver

    return current
