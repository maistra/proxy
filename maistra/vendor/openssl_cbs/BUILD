licenses(["notice"])  # Apache 2

package(default_visibility = ["//visibility:public"])

exports_files(["LICENSE"])

cc_library(
    name = "openssl_cbs_lib",
    srcs = [
        "opensslcbs/cbs.cc",
    ],
    hdrs = [
        "opensslcbs/cbs.h",
    ],
    deps = [
        "//external:abseil_strings",
        "//external:abseil_time",
        "//external:bssl_wrapper_lib",
        "@openssl//:openssl-lib",
    ],
)
