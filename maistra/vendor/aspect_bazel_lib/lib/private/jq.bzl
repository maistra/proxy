"""Implementation for jq rule"""

load("//lib:stamping.bzl", "STAMP_ATTRS", "maybe_stamp")

_jq_attrs = dict({
    "srcs": attr.label_list(
        allow_files = True,
        mandatory = True,
        allow_empty = True,
    ),
    "filter": attr.string(),
    "filter_file": attr.label(allow_single_file = True),
    "args": attr.string_list(),
    "out": attr.output(),
    "_parse_status_file_filter": attr.label(
        allow_single_file = True,
        default = Label("//lib/private:parse_status_file.jq"),
    ),
}, **STAMP_ATTRS)

def _jq_impl(ctx):
    jq_bin = ctx.toolchains["@aspect_bazel_lib//lib:jq_toolchain_type"].jqinfo.bin

    out = ctx.outputs.out or ctx.actions.declare_file(ctx.attr.name + ".json")
    args = ctx.attr.args
    inputs = ctx.files.srcs[:]

    if not ctx.attr.filter and not ctx.attr.filter_file:
        fail("Must provide a filter or a filter_file")
    if ctx.attr.filter and ctx.attr.filter_file:
        fail("Cannot provide both a filter and a filter_file")

    # jq hangs when there are no input sources unless --null-input flag is passed
    if len(ctx.attr.srcs) == 0 and "-n" not in args and "--null-input" not in args:
        args = args + ["--null-input"]

    if ctx.attr.filter_file:
        args = args + ["--from-file '%s'" % ctx.file.filter_file.path]
        inputs.append(ctx.file.filter_file)

    stamp = maybe_stamp(ctx)
    if stamp:
        # create an action that gives a JSON representation of the stamp keys
        stamp_json = ctx.actions.declare_file("_%s_stamp.json" % ctx.label.name)
        ctx.actions.run_shell(
            tools = [jq_bin],
            inputs = [stamp.stable_status_file, stamp.volatile_status_file, ctx.file._parse_status_file_filter],
            outputs = [stamp_json],
            command = "{jq} -s -R -f {filter} {stable} {volatile} > {out}".format(
                jq = jq_bin.path,
                filter = ctx.file._parse_status_file_filter.path,
                stable = stamp.stable_status_file.path,
                volatile = stamp.volatile_status_file.path,
                out = stamp_json.path,
            ),
            mnemonic = "ConvertStatusToJson",
        )
        inputs.append(stamp_json)

        # jq says of --argfile:
        # > Do not use. Use --slurpfile instead.
        # > (This option is like --slurpfile, but when the file has just one text,
        # > then that is used, else an array of texts is used as in --slurpfile.)
        # However there's no indication that it's deprecated. Maybe it's a style convention.
        # For our purposes, "$STAMP.BUILD_TIMESTAMP" looks a lot more sensible in a BUILD file
        # than "$STAMP[0].BUILD_TIMESTAMP".
        args = args + ["--argfile", "STAMP", stamp_json.path]

    cmd = "{jq} {args} {filter} {sources} > {out}".format(
        jq = jq_bin.path,
        args = " ".join(args),
        filter = "'%s'" % ctx.attr.filter if ctx.attr.filter else "",
        sources = " ".join(["'%s'" % file.path for file in ctx.files.srcs]),
        out = out.path,
    )

    ctx.actions.run_shell(
        tools = [jq_bin],
        inputs = inputs,
        outputs = [out],
        command = cmd,
        mnemonic = "Jq",
    )

    return DefaultInfo(files = depset([out]), runfiles = ctx.runfiles([out]))

jq_lib = struct(
    attrs = _jq_attrs,
    implementation = _jq_impl,
)
