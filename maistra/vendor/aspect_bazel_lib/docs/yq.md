<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Public API for yq

<a id="yq"></a>

## yq

<pre>
yq(<a href="#yq-name">name</a>, <a href="#yq-srcs">srcs</a>, <a href="#yq-expression">expression</a>, <a href="#yq-args">args</a>, <a href="#yq-outs">outs</a>, <a href="#yq-kwargs">kwargs</a>)
</pre>

Invoke yq with an expression on a set of input files.

For yq documentation, see https://mikefarah.gitbook.io/yq.

To use this rule you must register the yq toolchain in your WORKSPACE:

```starlark
load("@aspect_bazel_lib//lib:repositories.bzl", "register_yq_toolchains")

register_yq_toolchains()
```

Usage examples:

```starlark
load("@aspect_bazel_lib//lib:yq.bzl", "yq")
```

```starlark
# Remove fields
yq(
    name = "safe-config",
    srcs = ["config.yaml"],
    expression = "del(.credentials)",
)
```

```starlark
# Merge two yaml documents
yq(
    name = "ab",
    srcs = [
        "a.yaml",
        "b.yaml",
    ],
    expression = ". as $item ireduce ({}; . * $item )",
)
```

```starlark
# Split a yaml file into several files
yq(
    name = "split",
    srcs = ["multidoc.yaml"],
    outs = [
        "first.yml",
        "second.yml",
    ],
    args = [
        "-s '.a'",  # Split expression
        "--no-doc", # Exclude document separator --
    ],
)
```

```starlark
# Convert a yaml file to json
yq(
    name = "convert-to-json",
    srcs = ["foo.yaml"],
    args = ["-o=json"],
    outs = ["foo.json"],
)
```

```starlark
# Convert a json file to yaml
yq(
    name = "convert-to-yaml",
    srcs = ["bar.json"],
    args = ["-P"],
    outs = ["bar.yaml"],
)
```

```starlark
# Call yq in a genrule
genrule(
    name = "generate",
    srcs = ["farm.yaml"],
    outs = ["genrule_output.yaml"],
    cmd = "$(YQ_BIN) '.moo = "cow"' $(location farm.yaml) > $@",
    toolchains = ["@yq_toolchains//:resolved_toolchain"],
)
```

yq is capable of parsing and outputting to other formats. See their [docs](https://mikefarah.gitbook.io/yq) for more examples.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="yq-name"></a>name |  Name of the rule   |  none |
| <a id="yq-srcs"></a>srcs |  List of input file labels   |  none |
| <a id="yq-expression"></a>expression |  yq expression (https://mikefarah.gitbook.io/yq/commands/evaluate). Defaults to the identity expression "."   |  <code>"."</code> |
| <a id="yq-args"></a>args |  Additional args to pass to yq. Note that you do not need to pass _eval_ or _eval-all_ as this is handled automatically based on the number <code>srcs</code>. Passing the output format or the parse format is optional as these can be guessed based on the file extensions in <code>srcs</code> and <code>outs</code>.   |  <code>[]</code> |
| <a id="yq-outs"></a>outs |  Name of the output files. Defaults to a single output with the name plus a ".yaml" extension, or the extension corresponding to a passed output argment (e.g., "-o=json"). For split operations you must declare all outputs as the name of the output files depends on the expression.   |  <code>None</code> |
| <a id="yq-kwargs"></a>kwargs |  Other common named parameters such as <code>tags</code> or <code>visibility</code>   |  none |


