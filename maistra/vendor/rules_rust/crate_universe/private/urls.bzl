"""A file containing urls and associated sha256 values for cargo-bazel binaries

This file is auto-generated for each release to match the urls and sha256s of
the binaries produced for it.
"""

# Example:
# {
#     "x86_64-unknown-linux-gnu": "https://domain.com/downloads/cargo-bazel-x86_64-unknown-linux-gnu",
#     "x86_64-apple-darwin": "https://domain.com/downloads/cargo-bazel-x86_64-apple-darwin",
#     "x86_64-pc-windows-msvc": "https://domain.com/downloads/cargo-bazel-x86_64-pc-windows-msvc",
# }
CARGO_BAZEL_URLS = {
  "aarch64-apple-darwin": "https://github.com/bazelbuild/rules_rust/releases/download/0.25.1/cargo-bazel-aarch64-apple-darwin",
  "aarch64-pc-windows-msvc": "https://github.com/bazelbuild/rules_rust/releases/download/0.25.1/cargo-bazel-aarch64-pc-windows-msvc.exe",
  "aarch64-unknown-linux-gnu": "https://github.com/bazelbuild/rules_rust/releases/download/0.25.1/cargo-bazel-aarch64-unknown-linux-gnu",
  "x86_64-apple-darwin": "https://github.com/bazelbuild/rules_rust/releases/download/0.25.1/cargo-bazel-x86_64-apple-darwin",
  "x86_64-pc-windows-gnu": "https://github.com/bazelbuild/rules_rust/releases/download/0.25.1/cargo-bazel-x86_64-pc-windows-gnu.exe",
  "x86_64-pc-windows-msvc": "https://github.com/bazelbuild/rules_rust/releases/download/0.25.1/cargo-bazel-x86_64-pc-windows-msvc.exe",
  "x86_64-unknown-linux-gnu": "https://github.com/bazelbuild/rules_rust/releases/download/0.25.1/cargo-bazel-x86_64-unknown-linux-gnu",
  "x86_64-unknown-linux-musl": "https://github.com/bazelbuild/rules_rust/releases/download/0.25.1/cargo-bazel-x86_64-unknown-linux-musl"
}

# Example:
# {
#     "x86_64-unknown-linux-gnu": "1d687fcc860dc8a1aa6198e531f0aee0637ed506d6a412fe2b9884ff5b2b17c0",
#     "x86_64-apple-darwin": "0363e450125002f581d29cf632cc876225d738cfa433afa85ca557afb671eafa",
#     "x86_64-pc-windows-msvc": "f5647261d989f63dafb2c3cb8e131b225338a790386c06cf7112e43dd9805882",
# }
CARGO_BAZEL_SHA256S = {
  "aarch64-apple-darwin": "e0756e4c11fe459502a8cbf7e4e0ccc6141f352d40274ac625753ec52b998c6d",
  "aarch64-pc-windows-msvc": "0c0d67d528bbb283dc8a020da5612c582b74436f197a34622806a91233261154",
  "aarch64-unknown-linux-gnu": "d28587856721782ad2878b20f24f5e01987d0079711c15bd3b98546d716421d1",
  "x86_64-apple-darwin": "6b80c992f3eb9860b63b5c2c25b6cd34ff90453e40ac9a87197fb6131f64e9d7",
  "x86_64-pc-windows-gnu": "75e3b0fd61a03e96ed7b49f009fb8a1a574cbedd7afa580afe81ec678805e9c1",
  "x86_64-pc-windows-msvc": "a51d0db5a0c5ce9622d0f87cf8828b7c15825a48558c05d9861563f65837f115",
  "x86_64-unknown-linux-gnu": "885c4bd890ace1cf35d19edbeaff4f7ceb99c57f28464d3e979d496a95648866",
  "x86_64-unknown-linux-musl": "d90e613e498e5202759c43260b07a5ddccdd5aa5c28036c8a1d6c211f7a97b8e"
}

# Example:
# Label("//crate_universe:cargo_bazel_bin")
CARGO_BAZEL_LABEL = Label("//crate_universe:cargo_bazel_bin")
