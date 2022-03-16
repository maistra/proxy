load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])  # Apache 2

cc_library(
    name = "openssl-lib",
    srcs = [
        "libcrypto.so.1.1",
        "libssl.so.1.1",
    ],
    linkstatic = False,
    visibility = ["//visibility:public"],
)
