load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

filegroup(
    name = "gen_ocsp_data",
    srcs = ["gen_unittest_ocsp_data.sh"],
)
