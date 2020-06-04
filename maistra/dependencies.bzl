def maistra_chromium_v8():
    native.bind(
        name = "wee8",
        actual = "@maistra_chromium_v8//:wee8",
    )
