load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "openssl_includes_lib",
    hdrs = [
        "e_os.h",
        "include/internal/dane.h",
        "include/internal/nelem.h",
        "include/internal/numbers.h",
        "include/internal/refcount.h",
        "include/internal/tsan_assist.h",
        "ssl/packet_locl.h",
        "ssl/record/record.h",
        "ssl/ssl_locl.h",
        "ssl/statem/statem.h",
    ],
    copts = ["-Wno-error=error"],
    includes = [
        "include",
        "ssl",
        "ssl/record",
        "ssl/statem",
    ],
    visibility = ["//visibility:public"],
)
