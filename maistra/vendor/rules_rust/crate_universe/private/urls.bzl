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
  "aarch64-apple-darwin": "https://github.com/bazelbuild/rules_rust/releases/download/0.18.0/cargo-bazel-aarch64-apple-darwin",
  "aarch64-pc-windows-msvc": "https://github.com/bazelbuild/rules_rust/releases/download/0.18.0/cargo-bazel-aarch64-pc-windows-msvc.exe",
  "aarch64-unknown-linux-gnu": "https://github.com/bazelbuild/rules_rust/releases/download/0.18.0/cargo-bazel-aarch64-unknown-linux-gnu",
  "x86_64-apple-darwin": "https://github.com/bazelbuild/rules_rust/releases/download/0.18.0/cargo-bazel-x86_64-apple-darwin",
  "x86_64-pc-windows-gnu": "https://github.com/bazelbuild/rules_rust/releases/download/0.18.0/cargo-bazel-x86_64-pc-windows-gnu.exe",
  "x86_64-pc-windows-msvc": "https://github.com/bazelbuild/rules_rust/releases/download/0.18.0/cargo-bazel-x86_64-pc-windows-msvc.exe",
  "x86_64-unknown-linux-gnu": "https://github.com/bazelbuild/rules_rust/releases/download/0.18.0/cargo-bazel-x86_64-unknown-linux-gnu",
  "x86_64-unknown-linux-musl": "https://github.com/bazelbuild/rules_rust/releases/download/0.18.0/cargo-bazel-x86_64-unknown-linux-musl"
}

# Example:
# {
#     "x86_64-unknown-linux-gnu": "1d687fcc860dc8a1aa6198e531f0aee0637ed506d6a412fe2b9884ff5b2b17c0",
#     "x86_64-apple-darwin": "0363e450125002f581d29cf632cc876225d738cfa433afa85ca557afb671eafa",
#     "x86_64-pc-windows-msvc": "f5647261d989f63dafb2c3cb8e131b225338a790386c06cf7112e43dd9805882",
# }
CARGO_BAZEL_SHA256S = {
  "aarch64-apple-darwin": "98cede25600628fca842ec91d6faa151078ccf94961b20d3f46ea51cf545bb6b",
  "aarch64-pc-windows-msvc": "b385c21e3f5238fd486a259a3c5512f8b0731d753ca6814aefcb00878582ff5f",
  "aarch64-unknown-linux-gnu": "05715f81c4c9f690b1757da4a297c0cf3dffc5f44508b62a4ddd6ecb14673d28",
  "x86_64-apple-darwin": "21f0ae67f52aa9428a88b59d688d0bd6745985ba99d673d5dc58854ddd6d585f",
  "x86_64-pc-windows-gnu": "1d2ab62afdec967eac3928ce416e8dd7cb98529d11b7eebadc40caf8f025de0b",
  "x86_64-pc-windows-msvc": "e7ca3e72d8b8cf7af535b9a3a97b5b8939b22cdce2321204e87cf44fb47b88ef",
  "x86_64-unknown-linux-gnu": "1122ea0e832f250ff95a93ac52ed6519546bfe92befd54de45834860577e099b",
  "x86_64-unknown-linux-musl": "7f8b4679c645c5b7099d6a615b321a1d889c83dc32bbb0c0e49898ce854b617a"
}

# Example:
# Label("//crate_universe:cargo_bazel_bin")
CARGO_BAZEL_LABEL = Label("//crate_universe:cargo_bazel_bin")
