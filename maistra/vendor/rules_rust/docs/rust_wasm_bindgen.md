<!-- Generated with Stardoc: http://skydoc.bazel.build -->
# Rust Wasm Bindgen

* [rust_wasm_bindgen_repositories](#rust_wasm_bindgen_repositories)
* [rust_wasm_bindgen_toolchain](#rust_wasm_bindgen_toolchain)
* [rust_wasm_bindgen](#rust_wasm_bindgen)


## Overview

To build a `rust_binary` for `wasm32-unknown-unknown` target add the `--platforms=@rules_rust//rust/platform:wasm` flag.

```command
bazel build @examples//hello_world_wasm --platforms=@rules_rust//rust/platform:wasm
```

To build a `rust_binary` for `wasm32-wasi` target add the `--platforms=@rules_rust//rust/platform:wasi` flag.

```command
bazel build @examples//hello_world_wasm --platforms=@rules_rust//rust/platform:wasi
```

`rust_wasm_bindgen` will automatically transition to the `wasm` platform and can be used when
building WebAssembly code for the host target.


<a id="#rust_wasm_bindgen"></a>

## rust_wasm_bindgen

<pre>
rust_wasm_bindgen(<a href="#rust_wasm_bindgen-name">name</a>, <a href="#rust_wasm_bindgen-bindgen_flags">bindgen_flags</a>, <a href="#rust_wasm_bindgen-target">target</a>, <a href="#rust_wasm_bindgen-wasm_file">wasm_file</a>)
</pre>

Generates javascript and typescript bindings for a webassembly module using [wasm-bindgen][ws].

[ws]: https://rustwasm.github.io/docs/wasm-bindgen/

To use the Rust WebAssembly bindgen rules, add the following to your `WORKSPACE` file to add the
external repositories for the Rust bindgen toolchain (in addition to the Rust rules setup):

```python
load("@rules_rust//wasm_bindgen:repositories.bzl", "rust_wasm_bindgen_repositories")

rust_wasm_bindgen_repositories()
```

For more details on `rust_wasm_bindgen_repositories`, see [here](#rust_wasm_bindgen_repositories).

An example of this rule in use can be seen at [@rules_rust//examples/wasm](../examples/wasm)


**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_wasm_bindgen-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/docs/build-ref.html#name">Name</a> | required |  |
| <a id="rust_wasm_bindgen-bindgen_flags"></a>bindgen_flags |  Flags to pass directly to the bindgen executable. See https://github.com/rustwasm/wasm-bindgen/ for details.   | List of strings | optional | [] |
| <a id="rust_wasm_bindgen-target"></a>target |  The type of output to generate. See https://rustwasm.github.io/wasm-bindgen/reference/deployment.html for details.   | String | optional | "bundler" |
| <a id="rust_wasm_bindgen-wasm_file"></a>wasm_file |  The .wasm file to generate bindings for.   | <a href="https://bazel.build/docs/build-ref.html#labels">Label</a> | optional | None |


<a id="#rust_wasm_bindgen_toolchain"></a>

## rust_wasm_bindgen_toolchain

<pre>
rust_wasm_bindgen_toolchain(<a href="#rust_wasm_bindgen_toolchain-name">name</a>, <a href="#rust_wasm_bindgen_toolchain-bindgen">bindgen</a>)
</pre>

The tools required for the `rust_wasm_bindgen` rule.

In cases where users want to control or change the version of `wasm-bindgen` used by [rust_wasm_bindgen](#rust_wasm_bindgen),
a unique toolchain can be created as in the example below:

```python
load("@rules_rust//bindgen:bindgen.bzl", "rust_bindgen_toolchain")

rust_bindgen_toolchain(
    bindgen = "//my/cargo_raze:cargo_bin_wasm_bindgen",
)

toolchain(
    name = "wasm_bindgen_toolchain",
    toolchain = "wasm_bindgen_toolchain_impl",
    toolchain_type = "@rules_rust//wasm_bindgen:wasm_bindgen_toolchain",
)
```

Now that you have your own toolchain, you need to register it by
inserting the following statement in your `WORKSPACE` file:

```python
register_toolchains("//my/toolchains:wasm_bindgen_toolchain")
```

For additional information, see the [Bazel toolchains documentation][toolchains].

[toolchains]: https://docs.bazel.build/versions/master/toolchains.html


**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_wasm_bindgen_toolchain-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/docs/build-ref.html#name">Name</a> | required |  |
| <a id="rust_wasm_bindgen_toolchain-bindgen"></a>bindgen |  The label of a <code>wasm-bindgen-cli</code> executable.   | <a href="https://bazel.build/docs/build-ref.html#labels">Label</a> | optional | None |


<a id="#rust_wasm_bindgen_repositories"></a>

## rust_wasm_bindgen_repositories

<pre>
rust_wasm_bindgen_repositories(<a href="#rust_wasm_bindgen_repositories-register_default_toolchain">register_default_toolchain</a>)
</pre>

Declare dependencies needed for [rust_wasm_bindgen](#rust_wasm_bindgen).

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="rust_wasm_bindgen_repositories-register_default_toolchain"></a>register_default_toolchain |  If True, the default [rust_wasm_bindgen_toolchain](#rust_wasm_bindgen_toolchain) (<code>@rules_rust//wasm_bindgen:default_wasm_bindgen_toolchain</code>) is registered. This toolchain requires a set of dependencies that were generated using [cargo raze](https://github.com/google/cargo-raze). These will also be loaded.   |  <code>True</code> |


