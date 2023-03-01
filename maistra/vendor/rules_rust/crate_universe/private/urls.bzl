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
  "aarch64-unknown-linux-gnu": "https://github.com/bazelbuild/rules_rust/releases/download/0.8.1/cargo-bazel-aarch64-unknown-linux-gnu",
  "x86_64-apple-darwin": "https://github.com/bazelbuild/rules_rust/releases/download/0.8.1/cargo-bazel-x86_64-apple-darwin",
  "x86_64-pc-windows-gnu": "https://github.com/bazelbuild/rules_rust/releases/download/0.8.1/cargo-bazel-x86_64-pc-windows-gnu.exe",
  "x86_64-unknown-linux-gnu": "https://github.com/bazelbuild/rules_rust/releases/download/0.8.1/cargo-bazel-x86_64-unknown-linux-gnu",
  "aarch64-apple-darwin": "https://github.com/bazelbuild/rules_rust/releases/download/0.8.1/cargo-bazel-aarch64-apple-darwin",
  "x86_64-unknown-linux-musl": "https://github.com/bazelbuild/rules_rust/releases/download/0.8.1/cargo-bazel-x86_64-unknown-linux-musl",
  "x86_64-pc-windows-msvc": "https://github.com/bazelbuild/rules_rust/releases/download/0.8.1/cargo-bazel-x86_64-pc-windows-msvc.exe"
}

# Example:
# {
#     "x86_64-unknown-linux-gnu": "1d687fcc860dc8a1aa6198e531f0aee0637ed506d6a412fe2b9884ff5b2b17c0",
#     "x86_64-apple-darwin": "0363e450125002f581d29cf632cc876225d738cfa433afa85ca557afb671eafa",
#     "x86_64-pc-windows-msvc": "f5647261d989f63dafb2c3cb8e131b225338a790386c06cf7112e43dd9805882",
# }
CARGO_BAZEL_SHA256S = {
  "aarch64-unknown-linux-gnu": "03c4f14552edc9f441b24f303532e67907051d45dd4dc58f030ba1845f821c88",
  "x86_64-unknown-linux-musl": "5f7e460f436c5850bf1427cf61e6f096fee39cf9ae25abf9fc3f89e270f2b77e",
  "aarch64-apple-darwin": "01121781048c14f4f5ba4f1e20ca18aeeeb4dd8190327052f4690a11d7175978",
  "x86_64-pc-windows-msvc": "ad81389a1581752569544636c58f30d2c152c874fe761c9a18c077d5611240fa",
  "x86_64-apple-darwin": "3ebce8142103a38e841d7a3f9b43218459eadf5e6b89ac0ea56f0e44e040186b",
  "x86_64-unknown-linux-gnu": "e5b017d91e326d2d4351416d9d4f2a035bd7453b3802dcb1248a6bca9e42b39c",
  "x86_64-pc-windows-gnu": "e48de5d8f396c750c24b6829c1ae55d9b05fe2701506a3f33a428a01a06198bf"
}

# Example:
# Label("//crate_universe:cargo_bazel_bin")
CARGO_BAZEL_LABEL = Label("//crate_universe:cargo_bazel_bin")
