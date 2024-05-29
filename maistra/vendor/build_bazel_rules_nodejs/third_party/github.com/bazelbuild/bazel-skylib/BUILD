# bazel_skylib mocked out
# load("@bazel_skylib//:bzl_library.bzl", "bzl_library")
load("@build_bazel_rules_nodejs//:index.bzl", bzl_library = "dummy_bzl_library")

licenses(["notice"])

exports_files(["tests/unittest.bash"])

filegroup(
    name = "package_contents",
    srcs = glob(
        ["**"],
        exclude = ["tests/**"],
    ),
    visibility = ["//:__pkg__"],
)

bzl_library(
    name = "bzl",
    srcs = glob([
        "lib/*.bzl",
        "rules/**/*.bzl",
    ]),
    visibility = ["//visibility:public"],
    deps = [
        "//internal/npm_install:bzl",
    ],
)
