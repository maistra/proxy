load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")
load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies", "go_repository")
load("@build_bazel_rules_apple//apple:repositories.bzl", "apple_rules_dependencies")
load("@rules_fuzzing//fuzzing:repositories.bzl", "rules_fuzzing_dependencies")
load("@upb//bazel:workspace_deps.bzl", "upb_deps")
load("@rules_rust//rust:repositories.bzl", "rules_rust_dependencies", "rust_register_toolchains")
load("@rules_antlr//antlr:deps.bzl", "antlr_dependencies")
load("@proxy_wasm_rust_sdk//bazel:dependencies.bzl", "proxy_wasm_rust_sdk_dependencies")
load("@rules_cc//cc:repositories.bzl", "rules_cc_dependencies", "rules_cc_toolchains")
load("@base_pip3//:requirements.bzl", pip_dependencies = "install_deps")
load("@emsdk//:emscripten_deps.bzl", "emscripten_deps")

# go version for rules_go
GO_VERSION = "host"

def envoy_dependency_imports(go_version = GO_VERSION):
    # TODO: allow building of tools for easier onboarding
    rules_foreign_cc_dependencies(register_default_tools = False, register_built_tools = False)
    go_rules_dependencies()
    go_register_toolchains(go_version)
    gazelle_dependencies()
    apple_rules_dependencies()
    pip_dependencies()
    rules_rust_dependencies()
    rust_register_toolchains(
        include_rustc_srcs = True,
        extra_target_triples = [
            "aarch64-apple-ios",
            "aarch64-linux-android",
            "armv7-linux-androideabi",
            "i686-linux-android",
            "wasm32-unknown-unknown",
            "wasm32-wasi",
            "x86_64-apple-ios",
            "x86_64-linux-android",
        ],
    )
    upb_deps()
    antlr_dependencies(472)
    proxy_wasm_rust_sdk_dependencies()
    rules_fuzzing_dependencies(
        oss_fuzz = True,
        honggfuzz = False,
    )
    rules_cc_dependencies()
    rules_cc_toolchains()
    emscripten_deps()

    # These dependencies, like most of the Go in this repository, exist only for the API.
    go_repository(
        name = "org_golang_google_grpc",
        build_file_proto_mode = "disable",
        importpath = "google.golang.org/grpc",
        sum = "h1:raiipEjMOIC/TO2AvyTxP25XFdLxNIBwzDh3FM3XztI=",
        version = "v1.34.0",
        # project_url = "https://pkg.go.dev/google.golang.org/grpc",
        # last_update = "2020-12-02"
        # use_category = ["api"],
        # cpe = "cpe:2.3:a:grpc:grpc:*",
    )
    go_repository(
        name = "org_golang_x_net",
        importpath = "golang.org/x/net",
        sum = "h1:0mm1VjtFUOIlE1SbDlwjYaDxZVDP2S5ou6y0gSgXHu8=",
        version = "v0.0.0-20200226121028-0de0cce0169b",
        # project_url = "https://pkg.go.dev/golang.org/x/net",
        # last_update = "2020-02-26"
        # use_category = ["api"],
        # source = "https://github.com/envoyproxy/protoc-gen-validate/blob/v0.6.1/dependencies.bzl#L129-L134"
    )
    go_repository(
        name = "org_golang_x_text",
        importpath = "golang.org/x/text",
        sum = "h1:cokOdA+Jmi5PJGXLlLllQSgYigAEfHXJAERHVMaCc2k=",
        version = "v0.3.3",
        # project_url = "https://pkg.go.dev/golang.org/x/text",
        # last_update = "2021-06-16"
        # use_category = ["api"],
        # source = "https://github.com/envoyproxy/protoc-gen-validate/blob/v0.6.1/dependencies.bzl#L148-L153"
    )
    go_repository(
        name = "com_github_spf13_afero",
        importpath = "github.com/spf13/afero",
        sum = "h1:8q6vk3hthlpb2SouZcnBVKboxWQWMDNF38bwholZrJc=",
        version = "v1.3.4",
        # project_url = "https://pkg.go.dev/github.com/spf13/afero",
        # last_update = "2021-03-20"
        # use_category = ["api"],
        # source = "https://github.com/envoyproxy/protoc-gen-validate/blob/v0.6.1/dependencies.bzl#L60-L65"
    )
    go_repository(
        name = "com_github_lyft_protoc_gen_star",
        importpath = "github.com/lyft/protoc-gen-star",
        sum = "h1:xOpFu4vwmIoUeUrRuAtdCrZZymT/6AkW/bsUWA506Fo=",
        version = "v0.6.0",
        # project_url = "https://pkg.go.dev/github.com/lyft/protoc-gen-star",
        # last_update = "2022-03-04"
        # use_category = ["api"],
        # source = "https://github.com/envoyproxy/protoc-gen-validate/blob/v0.6.7/dependencies.bzl#L35-L40"
    )
    go_repository(
        name = "com_github_iancoleman_strcase",
        importpath = "github.com/iancoleman/strcase",
        sum = "h1:ux/56T2xqZO/3cP1I2F86qpeoYPCOzk+KF/UH/Ar+lk=",
        version = "v0.0.0-20180726023541-3605ed457bf7",
        # project_url = "https://pkg.go.dev/github.com/iancoleman/strcase",
        # last_update = "2020-11-22"
        # use_category = ["api"],
        # source = "https://github.com/envoyproxy/protoc-gen-validate/blob/v0.6.1/dependencies.bzl#L23-L28"
    )
