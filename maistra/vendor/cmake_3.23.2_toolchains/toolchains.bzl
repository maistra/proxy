def register_toolchains():
    native.register_toolchain(
    "@cmake-3.23.2-linux-aarch64//:cmake_tool",
    "@cmake-3.23.2-linux-x86_64//:cmake_tool",
    "@cmake-3.23.2-macos-universal//:cmake_tool",
    "@cmake-3.23.2-windows-i386//:cmake_tool",
    "@cmake-3.23.2-windows-x86_64//:cmake_tool",
    )
