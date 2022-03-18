# Rust Protobuf Rules

<div class="toc">
  <h2>Rules</h2>
  <ul>
    <li><a href="docs/index.md#rust_proto_library">rust_proto_library</a></li>
    <li><a href="docs/index.md#rust_grpc_library">rust_grpc_library</a></li>
  </ul>
</div>

## Overview

These build rules are used for building [protobufs][protobuf]/[gRPC][grpc] in [Rust][rust] with Bazel.

[rust]: http://www.rust-lang.org/
[protobuf]: https://developers.google.com/protocol-buffers/
[grpc]: https://grpc.io

See the [protobuf example](../examples/proto) for a more complete example of use.

### Setup

To use the Rust proto rules, add the following to your `WORKSPACE` file to add the
external repositories for the Rust proto toolchain (in addition to the [rust rules setup](..)):

```python
load("@rules_rust//proto:repositories.bzl", "rust_proto_repositories")

rust_proto_repositories()
```

[raze]: https://github.com/google/cargo-raze

This will load crate dependencies of protobuf that are generated using
[cargo raze][raze] inside the rules_rust
repository. However, using those dependencies might conflict with other uses
of [cargo raze][raze]. If you need to change
those dependencies, please see the [dedicated section below](#custom-deps).

For additional information about Bazel toolchains, see [here](https://docs.bazel.build/versions/master/toolchains.html).

## <a name="custom-deps">Customizing dependencies

These rules depends on the [`protobuf`](https://crates.io/crates/protobuf) and
the [`grpc`](https://crates.io/crates/grpc) crates in addition to the [protobuf
compiler](https://github.com/google/protobuf). To obtain these crates,
`rust_proto_repositories` imports the given crates using BUILD files generated with
[`cargo raze`][raze].

If you want to either change the protobuf and gRPC rust compilers, or to
simply use [`cargo raze`][raze] in a more
complex scenario (with more dependencies), you must redefine those
dependencies.

To do this, once you've imported the needed dependencies (see our
[Cargo.toml](raze/Cargo.toml) file to see the default dependencies), you
need to create your own toolchain. To do so you can create a BUILD
file with your toolchain definition, for example:

```python
load("@rules_rust//proto:toolchain.bzl", "rust_proto_toolchain")

rust_proto_toolchain(
    name = "proto-toolchain-impl",
    # Path to the protobuf compiler.
    protoc = "@com_google_protobuf//:protoc",
    # Protobuf compiler plugin to generate rust gRPC stubs.
    grpc_plugin = "//cargo_raze/remote:cargo_bin_protoc_gen_rust_grpc",
    # Protobuf compiler plugin to generate rust protobuf stubs.
    proto_plugin = "//cargo_raze/remote:cargo_bin_protoc_gen_rust",
)

toolchain(
    name = "proto-toolchain",
    toolchain = ":proto-toolchain-impl",
    toolchain_type = "@rules_rust//proto:toolchain",
)
```

Now that you have your own toolchain, you need to register it by
inserting the following statement in your `WORKSPACE` file:

```python
register_toolchains("//my/toolchains:proto-toolchain")
```

Finally, you might want to set the `rust_deps` attribute in
`rust_proto_library` and `rust_grpc_library` to change the compile-time
dependencies:

```python
rust_proto_library(
    ...
    rust_deps = ["//cargo_raze/remote:protobuf"],
    ...
)

rust_grpc_library(
    ...
    rust_deps = [
        "//cargo_raze/remote:protobuf",
        "//cargo_raze/remote:grpc",
        "//cargo_raze/remote:tls_api",
        "//cargo_raze/remote:tls_api_stub",
    ],
    ...
)
```

__Note__: Ideally, we would inject those dependencies from the toolchain,
but due to [bazelbuild/bazel#6889](https://github.com/bazelbuild/bazel/issues/6889)
all dependencies added via the toolchain ends-up being in the wrong
configuration.
