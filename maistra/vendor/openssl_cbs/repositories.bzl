load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def bsslwrapper_repositories(bind = True):
    http_archive(
        name = "bssl_wrapper",
        strip_prefix = "bssl_wrapper-34df33add45e1a02927fcf79b0bdd5899b7e2e36",
        url = "https://github.com/bdecoste/bssl_wrapper/archive/34df33add45e1a02927fcf79b0bdd5899b7e2e36.tar.gz",
        sha256 = "d9e500e1a8849c81e690966422baf66016a7ff85d044c210ad85644f62827158",
    )

    if bind:
        native.bind(
            name = "bssl_wrapper_lib",
            actual = "@bssl_wrapper//:bssl_wrapper_lib",
        )

ABSEIL_COMMIT = "cc8dcd307b76a575d2e3e0958a4fe4c7193c2f68"  # same as Envoy
ABSEIL_SHA256 = "e35082e88b9da04f4d68094c05ba112502a5063712f3021adfa465306d238c76"

def abseil_repositories(bind = True):
    http_archive(
        name = "com_google_absl",
        strip_prefix = "abseil-cpp-" + ABSEIL_COMMIT,
        url = "https://github.com/abseil/abseil-cpp/archive/" + ABSEIL_COMMIT + ".tar.gz",
        sha256 = ABSEIL_SHA256,
    )

    if bind:
        native.bind(
            name = "abseil_strings",
            actual = "@com_google_absl//absl/strings:strings",
        )
        native.bind(
            name = "abseil_time",
            actual = "@com_google_absl//absl/time:time",
        )
