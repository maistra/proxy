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
  "x86_64-apple-darwin": "https://github.com/bazelbuild/rules_rust/releases/download/0.3.1/cargo-bazel-x86_64-apple-darwin",
  "x86_64-unknown-linux-gnu": "https://github.com/bazelbuild/rules_rust/releases/download/0.3.1/cargo-bazel-x86_64-unknown-linux-gnu",
  "aarch64-unknown-linux-gnu": "https://github.com/bazelbuild/rules_rust/releases/download/0.3.1/cargo-bazel-aarch64-unknown-linux-gnu",
  "x86_64-pc-windows-gnu": "https://github.com/bazelbuild/rules_rust/releases/download/0.3.1/cargo-bazel-x86_64-pc-windows-gnu.exe",
  "x86_64-unknown-linux-musl": "https://github.com/bazelbuild/rules_rust/releases/download/0.3.1/cargo-bazel-x86_64-unknown-linux-musl",
  "aarch64-apple-darwin": "https://github.com/bazelbuild/rules_rust/releases/download/0.3.1/cargo-bazel-aarch64-apple-darwin",
  "x86_64-pc-windows-msvc": "https://github.com/bazelbuild/rules_rust/releases/download/0.3.1/cargo-bazel-x86_64-pc-windows-msvc.exe"
}

# Example:
# {
#     "x86_64-unknown-linux-gnu": "1d687fcc860dc8a1aa6198e531f0aee0637ed506d6a412fe2b9884ff5b2b17c0",
#     "x86_64-apple-darwin": "0363e450125002f581d29cf632cc876225d738cfa433afa85ca557afb671eafa",
#     "x86_64-pc-windows-msvc": "f5647261d989f63dafb2c3cb8e131b225338a790386c06cf7112e43dd9805882",
# }
CARGO_BAZEL_SHA256S = {
  "x86_64-unknown-linux-gnu": "9b710aa30393e55646b47cccd1f5db5d76a7c3f3f2308d3976816302fe61e09e",
  "aarch64-apple-darwin": "9d6f75574b60054c21832df468074799460e0bfdb5a55176d1514e68e28fa513",
  "x86_64-unknown-linux-musl": "a0e50af34bd54ed98bf4a981c0592a524b81eff6747449c4902adf1ef036dde4",
  "aarch64-unknown-linux-gnu": "d6eb93814e7ff863b48aecb5192036dbcbee658c78f49d2b8f1612fee20cd369",
  "x86_64-apple-darwin": "0e044ca104902c8ba617d2d4505edd2de3b4217f590bc1e37d04469a22382683",
  "x86_64-pc-windows-gnu": "dd7ca1f92c511187ff9c682137f1a4a8d84cf0152361354c8fddd91a1a774e70",
  "x86_64-pc-windows-msvc": "c244fe6b7203246a1d8b55afa83e0ad792e6555f90201ced6350716b01561288"
}

# Example:
# Label("//crate_universe:cargo_bazel_bin")
CARGO_BAZEL_LABEL = Label("//crate_universe:cargo_bazel_bin")
