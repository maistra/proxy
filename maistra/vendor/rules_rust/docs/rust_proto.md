<!-- Generated with Stardoc: http://skydoc.bazel.build -->
# Rust Proto

* [rust_grpc_library](#rust_grpc_library)
* [rust_proto_library](#rust_proto_library)
* [rust_proto_repositories](#rust_proto_repositories)
* [rust_proto_toolchain](#rust_proto_toolchain)

<a id="#rust_grpc_library"></a>

## rust_grpc_library

<pre>
rust_grpc_library(<a href="#rust_grpc_library-name">name</a>, <a href="#rust_grpc_library-deps">deps</a>, <a href="#rust_grpc_library-rust_deps">rust_deps</a>)
</pre>

Builds a Rust library crate from a set of `proto_library`s suitable for gRPC.

Example:

```python
load("//proto:proto.bzl", "rust_grpc_library")

proto_library(
    name = "my_proto",
    srcs = ["my.proto"]
)

rust_grpc_library(
    name = "rust",
    deps = [":my_proto"],
)

rust_binary(
    name = "my_service",
    srcs = ["my_service.rs"],
    deps = [":rust"],
)
```


**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_grpc_library-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/docs/build-ref.html#name">Name</a> | required |  |
| <a id="rust_grpc_library-deps"></a>deps |  List of proto_library dependencies that will be built. One crate for each proto_library will be created with the corresponding gRPC stubs.   | <a href="https://bazel.build/docs/build-ref.html#labels">List of labels</a> | required |  |
| <a id="rust_grpc_library-rust_deps"></a>rust_deps |  The crates the generated library depends on.   | <a href="https://bazel.build/docs/build-ref.html#labels">List of labels</a> | optional | [] |


<a id="#rust_proto_library"></a>

## rust_proto_library

<pre>
rust_proto_library(<a href="#rust_proto_library-name">name</a>, <a href="#rust_proto_library-deps">deps</a>, <a href="#rust_proto_library-rust_deps">rust_deps</a>)
</pre>

Builds a Rust library crate from a set of `proto_library`s.

Example:

```python
load("@rules_rust//proto:proto.bzl", "rust_proto_library")

proto_library(
    name = "my_proto",
    srcs = ["my.proto"]
)

proto_rust_library(
    name = "rust",
    deps = [":my_proto"],
)

rust_binary(
    name = "my_proto_binary",
    srcs = ["my_proto_binary.rs"],
    deps = [":rust"],
)
```


**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_proto_library-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/docs/build-ref.html#name">Name</a> | required |  |
| <a id="rust_proto_library-deps"></a>deps |  List of proto_library dependencies that will be built. One crate for each proto_library will be created with the corresponding stubs.   | <a href="https://bazel.build/docs/build-ref.html#labels">List of labels</a> | required |  |
| <a id="rust_proto_library-rust_deps"></a>rust_deps |  The crates the generated library depends on.   | <a href="https://bazel.build/docs/build-ref.html#labels">List of labels</a> | optional | [] |


<a id="#rust_proto_toolchain"></a>

## rust_proto_toolchain

<pre>
rust_proto_toolchain(<a href="#rust_proto_toolchain-name">name</a>, <a href="#rust_proto_toolchain-edition">edition</a>, <a href="#rust_proto_toolchain-grpc_compile_deps">grpc_compile_deps</a>, <a href="#rust_proto_toolchain-grpc_plugin">grpc_plugin</a>, <a href="#rust_proto_toolchain-proto_compile_deps">proto_compile_deps</a>,
                     <a href="#rust_proto_toolchain-proto_plugin">proto_plugin</a>, <a href="#rust_proto_toolchain-protoc">protoc</a>)
</pre>

Declares a Rust Proto toolchain for use.

This is used to configure proto compilation and can be used to set different protobuf compiler plugin.

Example:

Suppose a new nicer gRPC plugin has came out. The new plugin can be used in Bazel by defining a new toolchain definition and declaration:

```python
load('@rules_rust//proto:toolchain.bzl', 'rust_proto_toolchain')

rust_proto_toolchain(
   name="rust_proto_impl",
   grpc_plugin="@rust_grpc//:grpc_plugin",
   grpc_compile_deps=["@rust_grpc//:grpc_deps"],
)

toolchain(
    name="rust_proto",
    exec_compatible_with = [
        "@platforms//cpu:cpuX",
    ],
    target_compatible_with = [
        "@platforms//cpu:cpuX",
    ],
    toolchain = ":rust_proto_impl",
)
```

Then, either add the label of the toolchain rule to register_toolchains in the WORKSPACE, or pass it to the `--extra_toolchains` flag for Bazel, and it will be used.

See @rules_rust//proto:BUILD for examples of defining the toolchain.


**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_proto_toolchain-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/docs/build-ref.html#name">Name</a> | required |  |
| <a id="rust_proto_toolchain-edition"></a>edition |  The edition used by the generated rust source.   | String | optional | "2015" |
| <a id="rust_proto_toolchain-grpc_compile_deps"></a>grpc_compile_deps |  The crates the generated grpc libraries depends on.   | <a href="https://bazel.build/docs/build-ref.html#labels">List of labels</a> | optional | [Label("//proto/raze:protobuf"), Label("//proto/raze:grpc"), Label("//proto/raze:tls_api"), Label("//proto/raze:tls_api_stub")] |
| <a id="rust_proto_toolchain-grpc_plugin"></a>grpc_plugin |  The location of the Rust protobuf compiler plugin to generate rust gRPC stubs.   | <a href="https://bazel.build/docs/build-ref.html#labels">Label</a> | optional | //proto:protoc_gen_rust_grpc |
| <a id="rust_proto_toolchain-proto_compile_deps"></a>proto_compile_deps |  The crates the generated protobuf libraries depends on.   | <a href="https://bazel.build/docs/build-ref.html#labels">List of labels</a> | optional | [Label("//proto/raze:protobuf")] |
| <a id="rust_proto_toolchain-proto_plugin"></a>proto_plugin |  The location of the Rust protobuf compiler plugin used to generate rust sources.   | <a href="https://bazel.build/docs/build-ref.html#labels">Label</a> | optional | //proto:protoc_gen_rust |
| <a id="rust_proto_toolchain-protoc"></a>protoc |  The location of the <code>protoc</code> binary. It should be an executable target.   | <a href="https://bazel.build/docs/build-ref.html#labels">Label</a> | optional | @com_google_protobuf//:protoc |


<a id="#rust_proto_repositories"></a>

## rust_proto_repositories

<pre>
rust_proto_repositories(<a href="#rust_proto_repositories-register_default_toolchain">register_default_toolchain</a>)
</pre>

Declare dependencies needed for proto compilation.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="rust_proto_repositories-register_default_toolchain"></a>register_default_toolchain |  If True, the default [rust_proto_toolchain](#rust_proto_toolchain) (<code>@rules_rust//proto:default-proto-toolchain</code>) is registered. This toolchain requires a set of dependencies that were generated using [cargo raze](https://github.com/google/cargo-raze). These will also be loaded.   |  <code>True</code> |


