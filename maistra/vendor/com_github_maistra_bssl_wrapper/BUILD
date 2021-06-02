licenses(["notice"])  # Apache 2

package(default_visibility = ["//visibility:public"])

exports_files(["LICENSE"])

cc_library(
    name = "bssl_wrapper",
    srcs = [
	"include/bssl_wrapper/bssl_wrapper.h",
	"include/bssl_wrapper/openssl/digest.h",
	"include/bssl_wrapper/openssl/bn.h",
        "include/openssl/bytestring.h",
        "include/openssl/span.h",
	"src/internal.h",
	"src/bn.c",
	"src/digest.c",
        "src/cbs.c",
        "src/cbb.c",
	"src/asn1_a_int.c",
    ],
    copts = [
        "-std=c11",
        "-Wmissing-prototypes",
        "-Wold-style-definition",
        "-Wstrict-prototypes",

	# Assembler option --noexecstack adds .note.GNU-stack to each object to
        # ensure that binaries can be built with non-executable stack.
        "-Wa,--noexecstack",
        "-Wall",
        "-Werror",
        "-Wformat=2",
        "-Wsign-compare",
        "-Wmissing-field-initializers",
        "-Wwrite-strings",
        "-Wshadow",
        "-fno-common",
    ],
    includes = [
        "include",
    ],
    strip_include_prefix = "include",
    deps = [
        "@openssl//:openssl-lib",
    ],
)


