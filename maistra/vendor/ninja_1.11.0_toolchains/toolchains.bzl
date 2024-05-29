def register_toolchains():
    native.register_toolchain(
    "@ninja_1.11.0_linux//:ninja_tool",
    "@ninja_1.11.0_mac//:ninja_tool",
    "@ninja_1.11.0_win//:ninja_tool",
    )
