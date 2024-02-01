"""Rules for building protos in Rust with Prost and Tonic."""

load("@rules_proto//proto:defs.bzl", "ProtoInfo", "proto_common")
load("//rust:defs.bzl", "rust_common")

# buildifier: disable=bzl-visibility
load("//rust/private:rustc.bzl", "rustc_compile_action")

# buildifier: disable=bzl-visibility
load("//rust/private:utils.bzl", "can_build_metadata")

RUST_EDITION = "2021"

TOOLCHAIN_TYPE = "@rules_rust//proto/prost:toolchain_type"

ProstProtoInfo = provider(
    doc = "Rust Prost provider info",
    fields = {
        "dep_variant_info": "DepVariantInfo: For the compiled Rust gencode (also covers its " +
                            "transitive dependencies)",
        "package_info": "File: A newline delimited file of `--extern_path` values for protoc.",
        "transitive_dep_infos": "depset[DepVariantInfo]: Transitive dependencies of the compiled crate.",
    },
)

def _create_proto_lang_toolchain(ctx, prost_toolchain):
    proto_lang_toolchain = proto_common.ProtoLangToolchainInfo(
        out_replacement_format_flag = "--prost_out=%s",
        plugin_format_flag = prost_toolchain.prost_plugin_flag,
        plugin = prost_toolchain.prost_plugin[DefaultInfo].files_to_run,
        runtime = prost_toolchain.prost_runtime,
        provided_proto_sources = depset(),
        proto_compiler = ctx.attr._prost_process_wrapper[DefaultInfo].files_to_run,
        protoc_opts = prost_toolchain.protoc_opts,
        progress_message = "ProstGenProto %{label}",
        mnemonic = "ProstGenProto",
    )

    return proto_lang_toolchain

def _compile_proto(ctx, crate_name, proto_info, deps, prost_toolchain, rustfmt_toolchain = None):
    deps_info_file = ctx.actions.declare_file(ctx.label.name + ".prost_deps_info")
    dep_package_infos = [dep[ProstProtoInfo].package_info for dep in deps]
    ctx.actions.write(
        output = deps_info_file,
        content = "\n".join([file.path for file in dep_package_infos]),
    )

    package_info_file = ctx.actions.declare_file(ctx.label.name + ".prost_package_info")
    lib_rs = ctx.actions.declare_file("{}.lib.rs".format(ctx.label.name))

    proto_compiler = prost_toolchain.proto_compiler[DefaultInfo].files_to_run
    tools = depset([proto_compiler.executable])

    additional_args = ctx.actions.args()

    # Prost process wrapper specific args
    additional_args.add("--protoc={}".format(proto_compiler.executable.path))
    additional_args.add("--label={}".format(ctx.label))
    additional_args.add("--out_librs={}".format(lib_rs.path))
    additional_args.add("--package_info_output={}".format("{}={}".format(crate_name, package_info_file.path)))
    additional_args.add("--deps_info={}".format(deps_info_file.path))
    additional_args.add("--prost_opt=compile_well_known_types")
    additional_args.add("--descriptor_set={}".format(proto_info.direct_descriptor_set.path))
    additional_args.add_all(prost_toolchain.prost_opts, format_each = "--prost_opt=%s")

    if prost_toolchain.tonic_plugin:
        tonic_plugin = prost_toolchain.tonic_plugin[DefaultInfo].files_to_run
        additional_args.add(prost_toolchain.tonic_plugin_flag % tonic_plugin.executable.path)
        additional_args.add("--tonic_opt=no_include")
        additional_args.add("--tonic_opt=compile_well_known_types")
        additional_args.add("--is_tonic")
        additional_args.add_all(prost_toolchain.tonic_opts, format_each = "--tonic_opt=%s")
        tools = depset([tonic_plugin.executable], transitive = [tools])

    if rustfmt_toolchain:
        additional_args.add("--rustfmt={}".format(rustfmt_toolchain.rustfmt.path))
        tools = depset(transitive = [tools, rustfmt_toolchain.all_files])

    additional_inputs = depset([deps_info_file, proto_info.direct_descriptor_set] + [dep[ProstProtoInfo].package_info for dep in deps])

    proto_common.compile(
        actions = ctx.actions,
        proto_info = proto_info,
        additional_tools = tools.to_list(),
        additional_inputs = additional_inputs,
        additional_args = additional_args,
        generated_files = [lib_rs, package_info_file],
        proto_lang_toolchain_info = _create_proto_lang_toolchain(ctx, prost_toolchain),
        plugin_output = ctx.bin_dir.path,
    )

    return lib_rs, package_info_file

def _get_crate_info(providers):
    """Finds the CrateInfo provider in the list of providers."""
    for provider in providers:
        if hasattr(provider, "name"):
            return provider
    fail("Couldn't find a CrateInfo in the list of providers")

def _get_dep_info(providers):
    """Finds the DepInfo provider in the list of providers."""
    for provider in providers:
        if hasattr(provider, "direct_crates"):
            return provider
    fail("Couldn't find a DepInfo in the list of providers")

def _get_cc_info(providers):
    """Finds the CcInfo provider in the list of providers."""
    for provider in providers:
        if hasattr(provider, "linking_context"):
            return provider
    fail("Couldn't find a CcInfo in the list of providers")

def _compile_rust(ctx, attr, crate_name, src, deps, edition):
    """Compiles a Rust source file.

    Args:
      ctx (RuleContext): The rule context.
      attr (Attrs): The current rule's attributes (`ctx.attr` for rules, `ctx.rule.attr` for aspects)
      crate_name (str): The crate module name to use.
      src (File): The crate root source file to be compiled.
      deps (List of DepVariantInfo): A list of dependencies needed.
      edition (str): The Rust edition to use.

    Returns:
      A DepVariantInfo provider.
    """
    toolchain = ctx.toolchains["@rules_rust//rust:toolchain_type"]
    output_hash = repr(hash(src.path + ".prost"))

    lib_name = "{prefix}{name}-{lib_hash}{extension}".format(
        prefix = "lib",
        name = crate_name,
        lib_hash = output_hash,
        extension = ".rlib",
    )

    rmeta_name = "{prefix}{name}-{lib_hash}{extension}".format(
        prefix = "lib",
        name = crate_name,
        lib_hash = output_hash,
        extension = ".rmeta",
    )

    lib = ctx.actions.declare_file(lib_name)
    rmeta = None

    if can_build_metadata(toolchain, ctx, "rlib"):
        rmeta_name = "{prefix}{name}-{lib_hash}{extension}".format(
            prefix = "lib",
            name = crate_name,
            lib_hash = output_hash,
            extension = ".rmeta",
        )
        rmeta = ctx.actions.declare_file(rmeta_name)

    providers = rustc_compile_action(
        ctx = ctx,
        attr = attr,
        toolchain = toolchain,
        crate_info = rust_common.create_crate_info(
            name = crate_name,
            type = "rlib",
            root = src,
            srcs = depset([src]),
            deps = depset(deps),
            proc_macro_deps = depset([]),
            aliases = {},
            output = lib,
            metadata = rmeta,
            edition = edition,
            is_test = False,
            rustc_env = {},
            compile_data = depset([]),
            compile_data_targets = depset([]),
            owner = ctx.label,
        ),
        output_hash = output_hash,
    )

    crate_info = _get_crate_info(providers)
    dep_info = _get_dep_info(providers)
    cc_info = _get_cc_info(providers)

    return rust_common.dep_variant_info(
        crate_info = crate_info,
        dep_info = dep_info,
        cc_info = cc_info,
        build_info = None,
    )

def _rust_prost_aspect_impl(target, ctx):
    if ProstProtoInfo in target:
        return []

    runtime_deps = []

    rustfmt_toolchain = ctx.toolchains["@rules_rust//rust/rustfmt:toolchain_type"]
    prost_toolchain = ctx.toolchains["@rules_rust//proto/prost:toolchain_type"]
    for prost_runtime in [prost_toolchain.prost_runtime, prost_toolchain.tonic_runtime]:
        if not prost_runtime:
            continue
        if rust_common.crate_group_info in prost_runtime:
            crate_group_info = prost_runtime[rust_common.crate_group_info]
            runtime_deps.extend(crate_group_info.dep_variant_infos.to_list())
        else:
            runtime_deps.append(rust_common.dep_variant_info(
                crate_info = prost_runtime[rust_common.crate_info] if rust_common.crate_info in prost_runtime else None,
                dep_info = prost_runtime[rust_common.dep_info] if rust_common.dep_info in prost_runtime else None,
                cc_info = prost_runtime[CcInfo] if CcInfo in prost_runtime else None,
                build_info = None,
            ))

    proto_deps = getattr(ctx.rule.attr, "deps", [])

    direct_deps = []
    transitive_deps = []
    for proto_dep in proto_deps:
        proto_info = proto_dep[ProstProtoInfo]

        direct_deps.append(proto_info.dep_variant_info)
        transitive_deps.append(depset(
            [proto_info.dep_variant_info],
            transitive = [proto_info.transitive_dep_infos],
        ))

    deps = runtime_deps + direct_deps

    crate_name = ctx.label.name.replace("-", "_").replace("/", "_")

    proto_info = target[ProtoInfo]

    lib_rs, package_info_file = _compile_proto(
        ctx = ctx,
        crate_name = crate_name,
        proto_info = proto_info,
        deps = proto_deps,
        prost_toolchain = prost_toolchain,
        rustfmt_toolchain = rustfmt_toolchain,
    )

    dep_variant_info = _compile_rust(
        ctx = ctx,
        attr = ctx.rule.attr,
        crate_name = crate_name,
        src = lib_rs,
        deps = deps,
        edition = RUST_EDITION,
    )

    return [
        ProstProtoInfo(
            dep_variant_info = dep_variant_info,
            transitive_dep_infos = depset(transitive = transitive_deps),
            package_info = package_info_file,
        ),
    ]

rust_prost_aspect = aspect(
    doc = "An aspect used to generate and compile proto files with Prost.",
    implementation = _rust_prost_aspect_impl,
    attr_aspects = ["deps"],
    attrs = {
        "_cc_toolchain": attr.label(
            doc = (
                "In order to use find_cc_toolchain, your rule has to depend " +
                "on C++ toolchain. See `@rules_cc//cc:find_cc_toolchain.bzl` " +
                "docs for details."
            ),
            default = Label("@bazel_tools//tools/cpp:current_cc_toolchain"),
        ),
        "_collect_cc_coverage": attr.label(
            default = Label("//util:collect_coverage"),
            executable = True,
            cfg = "exec",
        ),
        "_error_format": attr.label(
            default = Label("//:error_format"),
        ),
        "_extra_exec_rustc_flag": attr.label(
            default = Label("//:extra_exec_rustc_flag"),
        ),
        "_extra_exec_rustc_flags": attr.label(
            default = Label("//:extra_exec_rustc_flags"),
        ),
        "_extra_rustc_flag": attr.label(
            default = Label("//:extra_rustc_flag"),
        ),
        "_extra_rustc_flags": attr.label(
            default = Label("//:extra_rustc_flags"),
        ),
        "_grep_includes": attr.label(
            allow_single_file = True,
            default = Label("@bazel_tools//tools/cpp:grep-includes"),
            cfg = "exec",
        ),
        "_process_wrapper": attr.label(
            doc = "A process wrapper for running rustc on all platforms.",
            default = Label("//util/process_wrapper"),
            executable = True,
            allow_single_file = True,
            cfg = "exec",
        ),
        "_prost_process_wrapper": attr.label(
            doc = "The wrapper script for the Prost protoc plugin.",
            cfg = "exec",
            executable = True,
            default = Label("//proto/prost/private:protoc_wrapper"),
        ),
    },
    fragments = ["cpp"],
    host_fragments = ["cpp"],
    toolchains = [
        TOOLCHAIN_TYPE,
        "@bazel_tools//tools/cpp:toolchain_type",
        "@rules_rust//rust:toolchain_type",
        "@rules_rust//rust/rustfmt:toolchain_type",
    ],
    incompatible_use_toolchain_transition = True,
)

def _rust_prost_library_impl(ctx):
    proto_dep = ctx.attr.proto
    rust_proto_info = proto_dep[ProstProtoInfo]
    dep_variant_info = rust_proto_info.dep_variant_info

    return [
        DefaultInfo(files = depset([dep_variant_info.crate_info.output])),
        rust_common.crate_group_info(
            dep_variant_infos = depset(
                [dep_variant_info],
                transitive = [rust_proto_info.transitive_dep_infos],
            ),
        ),
    ]

rust_prost_library = rule(
    doc = "A rule for generating a Rust library using Prost.",
    implementation = _rust_prost_library_impl,
    attrs = {
        "proto": attr.label(
            doc = "A `proto_library` target for which to generate Rust gencode.",
            providers = [ProtoInfo],
            aspects = [rust_prost_aspect],
            mandatory = True,
        ),
        "_collect_cc_coverage": attr.label(
            default = Label("@rules_rust//util:collect_coverage"),
            executable = True,
            cfg = "exec",
        ),
    },
)

def _rust_prost_toolchain_impl(ctx):
    tonic_attrs = [ctx.attr.tonic_plugin_flag, ctx.attr.tonic_plugin, ctx.attr.tonic_runtime]
    if any(tonic_attrs) and not all(tonic_attrs):
        fail("When one tonic attribute is added, all must be added")

    return [platform_common.ToolchainInfo(
        prost_opts = ctx.attr.prost_opts,
        prost_plugin = ctx.attr.prost_plugin,
        prost_plugin_flag = ctx.attr.prost_plugin_flag,
        prost_runtime = ctx.attr.prost_runtime,
        prost_types = ctx.attr.prost_types,
        proto_compiler = ctx.attr.proto_compiler,
        protoc_opts = ctx.fragments.proto.experimental_protoc_opts,
        tonic_opts = ctx.attr.tonic_opts,
        tonic_plugin = ctx.attr.tonic_plugin,
        tonic_plugin_flag = ctx.attr.tonic_plugin_flag,
        tonic_runtime = ctx.attr.tonic_runtime,
    )]

rust_prost_toolchain = rule(
    implementation = _rust_prost_toolchain_impl,
    doc = "Rust Prost toolchain rule.",
    fragments = ["proto"],
    attrs = {
        "prost_opts": attr.string_list(
            doc = "Additional options to add to Prost.",
        ),
        "prost_plugin": attr.label(
            doc = "Additional plugins to add to Prost.",
            cfg = "exec",
            executable = True,
            mandatory = True,
        ),
        "prost_plugin_flag": attr.string(
            doc = "Prost plugin flag format. (e.g. `--plugin=protoc-gen-prost=%s`)",
            mandatory = True,
            default = "--plugin=protoc-gen-prost=%s",
        ),
        "prost_runtime": attr.label(
            doc = "The Prost runtime crates to use.",
            providers = [[rust_common.crate_info], [rust_common.crate_group_info]],
            mandatory = True,
        ),
        "prost_types": attr.label(
            doc = "The Prost types crates to use.",
            providers = [[rust_common.crate_info], [rust_common.crate_group_info]],
            mandatory = True,
        ),
        "proto_compiler": attr.label(
            doc = "The protoc compiler to use.",
            cfg = "exec",
            executable = True,
            mandatory = True,
        ),
        "tonic_opts": attr.string_list(
            doc = "Additional options to add to Tonic.",
        ),
        "tonic_plugin": attr.label(
            doc = "Additional plugins to add to Tonic.",
            cfg = "exec",
            executable = True,
        ),
        "tonic_plugin_flag": attr.string(
            doc = "Tonic plugin flag format. (e.g. `--plugin=protoc-gen-tonic=%s`))",
            default = "--plugin=protoc-gen-tonic=%s",
        ),
        "tonic_runtime": attr.label(
            doc = "The Tonic runtime crates to use.",
            providers = [[rust_common.crate_info], [rust_common.crate_group_info]],
        ),
    },
)

def _current_prost_runtime_impl(ctx):
    toolchain = ctx.toolchains[TOOLCHAIN_TYPE]

    runtime_deps = []

    for target in [toolchain.prost_runtime, toolchain.prost_types]:
        if rust_common.crate_group_info in target:
            crate_group_info = target[rust_common.crate_group_info]
            runtime_deps.extend(crate_group_info.dep_variant_infos.to_list())
        else:
            runtime_deps.append(rust_common.dep_variant_info(
                crate_info = target[rust_common.crate_info] if rust_common.crate_info in target else None,
                dep_info = target[rust_common.dep_info] if rust_common.dep_info in target else None,
                cc_info = target[CcInfo] if CcInfo in target else None,
                build_info = None,
            ))

    return [rust_common.crate_group_info(
        dep_variant_infos = depset(runtime_deps),
    )]

current_prost_runtime = rule(
    doc = "A rule for accessing the current Prost toolchain components needed by the process wrapper",
    provides = [rust_common.crate_group_info],
    implementation = _current_prost_runtime_impl,
    toolchains = [TOOLCHAIN_TYPE],
)
