# Rust WebAssembly Bindgen Rules

<div class="toc">
  <h2>Rules</h2>
  <ul>
    <li><a href="docs/index.md#rust_wasm_bindgen_toolchain">rust_wasm_bindgen_toolchain</a></li>
    <li>rust_wasm_bindgen_library</li>
  </ul>
</div>

## Overview

These rules are for using [Wasm Bindgen][wasm_bindgen] to generate [Rust][rust] bindings to.

[rust]: http://www.rust-lang.org/
[wasm_bindgen]: https://github.com/rustwasm/wasm-bindgen

See the [wasm bindgen example](../examples/hello_world_wasm/BUILD#L25) for a more complete example of use.

### Setup

To use the Rust WebAssembly bindgen rules, add the following to your `WORKSPACE` file to add the
external repositories for the Rust bindgen toolchain (in addition to the [rust rules setup](..)):

```python
load("@io_bazel_rules_rust//wasm_bindgen:repositories.bzl", "rust_wasm_bindgen_repositories")

rust_wasm_bindgen_repositories()
```
This makes the default toolchain defined in [`@io_bazel_rules_rust`](./BUILD) available.

[raze]: https://github.com/google/cargo-raze

It will load crate dependencies of bindgen that are generated using
[cargo raze][raze] inside the rules_rust
repository. However, using those dependencies might conflict with other uses
of [cargo raze][raze]. If you need to change
those dependencies, please see the [dedicated section below](#custom-deps).

For additional information, see the [Bazel toolchains documentation](https://docs.bazel.build/versions/master/toolchains.html).

## <a name="custom-toolchains">Customizing toolchains

You can also use your own version of wasm-bindgen using the toolchain rules below:

```python
load("@io_bazel_rules_rust//bindgen:bindgen.bzl", "rust_bindgen_toolchain")

rust_bindgen_toolchain(
    bindgen = "//my/raze:cargo_bin_wasm_bindgen",
)

toolchain(
    name = "wasm-bindgen-toolchain",
    toolchain = "wasm-bindgen-toolchain-impl",
    toolchain_type = "@io_bazel_rules_rust//wasm_bindgen:wasm_bindgen_toolchain",
)
```

Now that you have your own toolchain, you need to register it by
inserting the following statement in your `WORKSPACE` file:

```python
register_toolchains("//my/toolchains:wasm-bindgen-toolchain")
```
