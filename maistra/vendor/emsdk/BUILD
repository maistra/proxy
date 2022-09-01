

package(default_visibility = ["//visibility:public"])

# Doesn't have any effect
# sh_binary(
#     name = "local_link_to_host",
#     srcs = ["local_link_to_host.sh"],
# )

genrule(
    name = "local_link_rule",
    # Dummy output file, just to make genrule happy
    outs = ["hello.txt"],
    cmd = "/work/maistra/vendor/emsdk/local_link_to_host.sh; echo hello world >$@",
)


config_setting(
    name = "linux",
    constraint_values = [
        "@platforms//os:linux",
        "@platforms//cpu:x86_64",
    ],
)

config_setting(
    name = "macos",
    constraint_values = [
        "@platforms//os:macos",
        "@platforms//cpu:x86_64",
    ],
)

config_setting(
    name = "macos_arm64",
    constraint_values = [
        "@platforms//os:macos",
        "@platforms//cpu:arm64",
    ],
)

config_setting(
    name = "windows",
    constraint_values = [
        "@platforms//os:windows",
        "@platforms//cpu:x86_64",
    ],
)

filegroup(  name = "empty", 
            srcs = ["local_link_rule"], 
        )


alias(
    name = "binaries",
    actual = select({
        ":linux": "@emscripten_bin_linux//:all",
        ":macos": "@emscripten_bin_mac//:all",
        ":macos_arm64": "@emscripten_bin_mac_arm64//:all",
        ":windows": "@emscripten_bin_win//:all",
        "//conditions:default": ":empty",
    }),
)

alias(
    name = "node_modules",
    actual = select({
        ":macos": "@emscripten_npm_mac//:node_modules",
        ":macos_arm64": "@emscripten_npm_mac//:node_modules",
        ":windows": "@emscripten_npm_win//:node_modules",
        "//conditions:default": ":empty",
    }),
)
