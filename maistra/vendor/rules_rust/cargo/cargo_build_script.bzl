# buildifier: disable=module-docstring
load("@bazel_tools//tools/build_defs/cc:action_names.bzl", "CPP_COMPILE_ACTION_NAME", "C_COMPILE_ACTION_NAME")
load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "find_cpp_toolchain")
load("//rust:defs.bzl", "rust_binary", "rust_common")

# buildifier: disable=bzl-visibility
load("//rust/private:rustc.bzl", "BuildInfo", "get_compilation_mode_opts", "get_linker_and_args")

# buildifier: disable=bzl-visibility
load("//rust/private:utils.bzl", "dedent", "expand_dict_value_locations", "find_cc_toolchain", "find_toolchain", "name_to_crate_name")

def get_cc_compile_args_and_env(cc_toolchain, feature_configuration):
    """Gather cc environment variables from the given `cc_toolchain`

    Args:
        cc_toolchain (cc_toolchain): The current rule's `cc_toolchain`.
        feature_configuration (FeatureConfiguration): Class used to construct command lines from CROSSTOOL features.

    Returns:
        tuple: A tuple of the following items:
            - (sequence): A flattened C command line flags for given action.
            - (sequence): A flattened CXX command line flags for given action.
            - (dict): C environment variables to be set for given action.
    """
    compile_variables = cc_common.create_compile_variables(
        feature_configuration = feature_configuration,
        cc_toolchain = cc_toolchain,
    )
    cc_c_args = cc_common.get_memory_inefficient_command_line(
        feature_configuration = feature_configuration,
        action_name = C_COMPILE_ACTION_NAME,
        variables = compile_variables,
    )
    cc_cxx_args = cc_common.get_memory_inefficient_command_line(
        feature_configuration = feature_configuration,
        action_name = CPP_COMPILE_ACTION_NAME,
        variables = compile_variables,
    )
    cc_env = cc_common.get_environment_variables(
        feature_configuration = feature_configuration,
        action_name = C_COMPILE_ACTION_NAME,
        variables = compile_variables,
    )
    return cc_c_args, cc_cxx_args, cc_env

def _build_script_impl(ctx):
    """The implementation for the `_build_script_run` rule.

    Args:
        ctx (ctx): The rules context object

    Returns:
        list: A list containing a BuildInfo provider
    """
    script = ctx.executable.script
    toolchain = find_toolchain(ctx)
    out_dir = ctx.actions.declare_directory(ctx.label.name + ".out_dir")
    env_out = ctx.actions.declare_file(ctx.label.name + ".env")
    dep_env_out = ctx.actions.declare_file(ctx.label.name + ".depenv")
    flags_out = ctx.actions.declare_file(ctx.label.name + ".flags")
    link_flags = ctx.actions.declare_file(ctx.label.name + ".linkflags")
    link_search_paths = ctx.actions.declare_file(ctx.label.name + ".linksearchpaths")  # rustc-link-search, propagated from transitive dependencies
    manifest_dir = "%s.runfiles/%s/%s" % (script.path, ctx.label.workspace_name or ctx.workspace_name, ctx.label.package)
    compilation_mode_opt_level = get_compilation_mode_opts(ctx, toolchain).opt_level

    streams = struct(
        stdout = ctx.actions.declare_file(ctx.label.name + ".stdout.log"),
        stderr = ctx.actions.declare_file(ctx.label.name + ".stderr.log"),
    )

    pkg_name = _name_to_pkg_name(ctx.label.name)

    toolchain_tools = [toolchain.all_files]

    cc_toolchain = find_cpp_toolchain(ctx)

    # Start with the default shell env, which contains any --action_env
    # settings passed in on the command line.
    env = dict(ctx.configuration.default_shell_env)

    env.update({
        "CARGO_CRATE_NAME": name_to_crate_name(pkg_name),
        "CARGO_MANIFEST_DIR": manifest_dir,
        "CARGO_PKG_NAME": pkg_name,
        "HOST": toolchain.exec_triple,
        "NUM_JOBS": "1",
        "OPT_LEVEL": compilation_mode_opt_level,
        "RUSTC": toolchain.rustc.path,
        "TARGET": toolchain.target_flag_value,
        # OUT_DIR is set by the runner itself, rather than on the action.
    })

    # This isn't exactly right, but Bazel doesn't have exact views of "debug" and "release", so...
    env.update({
        "DEBUG": {"dbg": "true", "fastbuild": "true", "opt": "false"}.get(ctx.var["COMPILATION_MODE"], "true"),
        "PROFILE": {"dbg": "debug", "fastbuild": "debug", "opt": "release"}.get(ctx.var["COMPILATION_MODE"], "unknown"),
    })

    if ctx.attr.version:
        version = ctx.attr.version.split("+")[0].split(".")
        patch = version[2].split("-") if len(version) > 2 else [""]
        env["CARGO_PKG_VERSION_MAJOR"] = version[0]
        env["CARGO_PKG_VERSION_MINOR"] = version[1] if len(version) > 1 else ""
        env["CARGO_PKG_VERSION_PATCH"] = patch[0]
        env["CARGO_PKG_VERSION_PRE"] = patch[1] if len(patch) > 1 else ""
        env["CARGO_PKG_VERSION"] = ctx.attr.version

    # Pull in env vars which may be required for the cc_toolchain to work (e.g. on OSX, the SDK version).
    # We hope that the linker env is sufficient for the whole cc_toolchain.
    cc_toolchain, feature_configuration = find_cc_toolchain(ctx)
    linker, link_args, linker_env = get_linker_and_args(ctx, ctx.attr, cc_toolchain, feature_configuration, None)
    env.update(**linker_env)
    env["LD"] = linker
    env["LDFLAGS"] = " ".join(link_args)

    # MSVC requires INCLUDE to be set
    cc_c_args, cc_cxx_args, cc_env = get_cc_compile_args_and_env(cc_toolchain, feature_configuration)
    include = cc_env.get("INCLUDE")
    if include:
        env["INCLUDE"] = include

    if cc_toolchain:
        toolchain_tools.append(cc_toolchain.all_files)

        cc_executable = cc_toolchain.compiler_executable
        if cc_executable:
            env["CC"] = cc_executable
            env["CXX"] = cc_executable
        ar_executable = cc_toolchain.ar_executable
        if ar_executable:
            env["AR"] = ar_executable
        if cc_toolchain.sysroot:
            env["SYSROOT"] = cc_toolchain.sysroot

        # Populate CFLAGS and CXXFLAGS that cc-rs relies on when building from source, in particular
        # to determine the deployment target when building for apple platforms (`macosx-version-min`
        # for example, itself derived from the `macos_minimum_os` Bazel argument).
        env["CFLAGS"] = " ".join(cc_c_args)
        env["CXXFLAGS"] = " ".join(cc_cxx_args)

    # Inform build scripts of rustc flags
    # https://github.com/rust-lang/cargo/issues/9600
    env["CARGO_ENCODED_RUSTFLAGS"] = "\\x1f".join([
        # Allow build scripts to locate the generated sysroot
        "--sysroot=${{pwd}}/{}".format(toolchain.sysroot),
    ] + ctx.attr.rustc_flags)

    for f in ctx.attr.crate_features:
        env["CARGO_FEATURE_" + f.upper().replace("-", "_")] = "1"

    env.update(expand_dict_value_locations(
        ctx,
        ctx.attr.build_script_env,
        getattr(ctx.attr, "data", []) +
        getattr(ctx.attr, "compile_data", []) +
        getattr(ctx.attr, "tools", []),
    ))

    tools = depset(
        direct = [
            script,
            ctx.executable._cargo_build_script_runner,
        ] + ctx.files.data + ctx.files.tools + ([toolchain.target_json] if toolchain.target_json else []),
        transitive = toolchain_tools,
    )

    links = ctx.attr.links or ""

    # dep_env_file contains additional environment variables coming from
    # direct dependency sys-crates' build scripts. These need to be made
    # available to the current crate build script.
    # See https://doc.rust-lang.org/cargo/reference/build-scripts.html#-sys-packages
    # for details.
    args = ctx.actions.args()
    args.add_all([
        script.path,
        links,
        out_dir.path,
        env_out.path,
        flags_out.path,
        link_flags.path,
        link_search_paths.path,
        dep_env_out.path,
        streams.stdout.path,
        streams.stderr.path,
    ])
    build_script_inputs = []
    for dep in ctx.attr.deps:
        if rust_common.dep_info in dep and dep[rust_common.dep_info].dep_env:
            dep_env_file = dep[rust_common.dep_info].dep_env
            args.add(dep_env_file.path)
            build_script_inputs.append(dep_env_file)
            for dep_build_info in dep[rust_common.dep_info].transitive_build_infos.to_list():
                build_script_inputs.append(dep_build_info.out_dir)

    ctx.actions.run(
        executable = ctx.executable._cargo_build_script_runner,
        arguments = [args],
        outputs = [out_dir, env_out, flags_out, link_flags, link_search_paths, dep_env_out, streams.stdout, streams.stderr],
        tools = tools,
        inputs = build_script_inputs,
        mnemonic = "CargoBuildScriptRun",
        progress_message = "Running Cargo build script {}".format(pkg_name),
        env = env,
    )

    return [
        BuildInfo(
            out_dir = out_dir,
            rustc_env = env_out,
            dep_env = dep_env_out,
            flags = flags_out,
            link_flags = link_flags,
            link_search_paths = link_search_paths,
        ),
        OutputGroupInfo(streams = depset([streams.stdout, streams.stderr])),
    ]

_build_script_run = rule(
    doc = (
        "A rule for running a crate's `build.rs` files to generate build information " +
        "which is then used to determine how to compile said crate."
    ),
    implementation = _build_script_impl,
    attrs = {
        "build_script_env": attr.string_dict(
            doc = "Environment variables for build scripts.",
        ),
        "crate_features": attr.string_list(
            doc = "The list of rust features that the build script should consider activated.",
        ),
        "data": attr.label_list(
            doc = "Data required by the build script.",
            allow_files = True,
        ),
        "deps": attr.label_list(
            doc = "The Rust dependencies of the crate",
            providers = [rust_common.dep_info],
        ),
        "links": attr.string(
            doc = "The name of the native library this crate links against.",
        ),
        "rustc_flags": attr.string_list(
            doc = dedent("""\
                List of compiler flags passed to `rustc`.

                These strings are subject to Make variable expansion for predefined
                source/output path variables like `$location`, `$execpath`, and 
                `$rootpath`. This expansion is useful if you wish to pass a generated
                file of arguments to rustc: `@$(location //package:target)`.
            """),
        ),
        # The source of truth will be the `cargo_build_script` macro until stardoc
        # implements documentation inheritence. See https://github.com/bazelbuild/stardoc/issues/27
        "script": attr.label(
            doc = "The binary script to run, generally a `rust_binary` target.",
            executable = True,
            allow_files = True,
            mandatory = True,
            cfg = "exec",
        ),
        "tools": attr.label_list(
            doc = "Tools required by the build script.",
            allow_files = True,
            cfg = "exec",
        ),
        "version": attr.string(
            doc = "The semantic version (semver) of the crate",
        ),
        "_cargo_build_script_runner": attr.label(
            executable = True,
            allow_files = True,
            default = Label("//cargo/cargo_build_script_runner:cargo_build_script_runner"),
            cfg = "exec",
        ),
        "_cc_toolchain": attr.label(
            default = Label("@bazel_tools//tools/cpp:current_cc_toolchain"),
        ),
    },
    fragments = ["cpp"],
    toolchains = [
        str(Label("//rust:toolchain")),
        "@bazel_tools//tools/cpp:toolchain_type",
    ],
    incompatible_use_toolchain_transition = True,
)

def cargo_build_script(
        name,
        crate_features = [],
        version = None,
        deps = [],
        build_script_env = {},
        data = [],
        tools = [],
        links = None,
        rustc_env = {},
        rustc_flags = [],
        visibility = None,
        tags = None,
        **kwargs):
    """Compile and execute a rust build script to generate build attributes

    This rules take the same arguments as rust_binary.

    Example:

    Suppose you have a crate with a cargo build script `build.rs`:

    ```output
    [workspace]/
        hello_lib/
            BUILD
            build.rs
            src/
                lib.rs
    ```

    Then you want to use the build script in the following:

    `hello_lib/BUILD`:
    ```python
    package(default_visibility = ["//visibility:public"])

    load("@rules_rust//rust:defs.bzl", "rust_binary", "rust_library")
    load("@rules_rust//cargo:cargo_build_script.bzl", "cargo_build_script")

    # This will run the build script from the root of the workspace, and
    # collect the outputs.
    cargo_build_script(
        name = "build_script",
        srcs = ["build.rs"],
        # Optional environment variables passed during build.rs compilation
        rustc_env = {
           "CARGO_PKG_VERSION": "0.1.2",
        },
        # Optional environment variables passed during build.rs execution.
        # Note that as the build script's working directory is not execroot,
        # execpath/location will return an absolute path, instead of a relative
        # one.
        build_script_env = {
            "SOME_TOOL_OR_FILE": "$(execpath @tool//:binary)"
        }
        # Optional data/tool dependencies
        data = ["@tool//:binary"],
    )

    rust_library(
        name = "hello_lib",
        srcs = [
            "src/lib.rs",
        ],
        deps = [":build_script"],
    )
    ```

    The `hello_lib` target will be build with the flags and the environment variables declared by the \
    build script in addition to the file generated by it.

    Args:
        name (str): The name for the underlying rule. This should be the name of the package being compiled, optionally with a suffix of _build_script.
        crate_features (list, optional): A list of features to enable for the build script.
        version (str, optional): The semantic version (semver) of the crate.
        deps (list, optional): The dependencies of the crate.
        build_script_env (dict, optional): Environment variables for build scripts.
        data (list, optional): Files needed by the build script.
        tools (list, optional): Tools (executables) needed by the build script.
        links (str, optional): Name of the native library this crate links against.
        rustc_env (dict, optional): Environment variables to set in rustc when compiling the build script.
        rustc_flags (list, optional): List of compiler flags passed to `rustc`.
        visibility (list of label, optional): Visibility to apply to the generated build script output.
        tags: (list of str, optional): Tags to apply to the generated build script output.
        **kwargs: Forwards to the underlying `rust_binary` rule.
    """

    # This duplicates the code in _build_script_impl because we need to make these available both when we invoke rustc (this code) and when we run the compiled build script (_build_script_impl).
    # https://github.com/bazelbuild/rules_rust/issues/661 will hopefully remove this duplication.
    rustc_env = dict(rustc_env)
    if "CARGO_PKG_NAME" not in rustc_env:
        rustc_env["CARGO_PKG_NAME"] = _name_to_pkg_name(name)
    if "CARGO_CRATE_NAME" not in rustc_env:
        rustc_env["CARGO_CRATE_NAME"] = name_to_crate_name(_name_to_pkg_name(name))

    binary_tags = [tag for tag in tags or []]
    if "manual" not in binary_tags:
        binary_tags.append("manual")

    rust_binary(
        name = name + "_",
        crate_features = crate_features,
        version = version,
        deps = deps,
        data = data,
        rustc_env = rustc_env,
        rustc_flags = rustc_flags,
        tags = binary_tags,
        **kwargs
    )
    _build_script_run(
        name = name,
        script = ":{}_".format(name),
        crate_features = crate_features,
        version = version,
        build_script_env = build_script_env,
        links = links,
        deps = deps,
        data = data,
        tools = tools,
        rustc_flags = rustc_flags,
        visibility = visibility,
        tags = tags,
    )

def _name_to_pkg_name(name):
    if name.endswith("_build_script"):
        return name[:-len("_build_script")]
    return name
