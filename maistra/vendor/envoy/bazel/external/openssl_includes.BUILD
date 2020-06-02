cc_library(
    name = "openssl_includes_lib",
    copts = ["-Wno-error=error"],
    hdrs = [
            "e_os.h", 
            "ssl/ssl_locl.h", 
            "ssl/packet_locl.h", 
            "ssl/record/record.h", 
            "ssl/statem/statem.h", 
            "include/internal/dane.h",
            "include/internal/nelem.h",
            "include/internal/numbers.h",
            "include/internal/refcount.h",
            "include/internal/tsan_assist.h",
    ],
    includes = ["ssl", "ssl/record", "ssl/statem", "include",],
    visibility = ["//visibility:public"],
)
