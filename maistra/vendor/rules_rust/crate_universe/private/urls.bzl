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
  "aarch64-apple-darwin": "https://github.com/bazelbuild/rules_rust/releases/download/0.26.0/cargo-bazel-aarch64-apple-darwin",
  "aarch64-pc-windows-msvc": "https://github.com/bazelbuild/rules_rust/releases/download/0.26.0/cargo-bazel-aarch64-pc-windows-msvc.exe",
  "aarch64-unknown-linux-gnu": "https://github.com/bazelbuild/rules_rust/releases/download/0.26.0/cargo-bazel-aarch64-unknown-linux-gnu",
  "x86_64-apple-darwin": "https://github.com/bazelbuild/rules_rust/releases/download/0.26.0/cargo-bazel-x86_64-apple-darwin",
  "x86_64-pc-windows-gnu": "https://github.com/bazelbuild/rules_rust/releases/download/0.26.0/cargo-bazel-x86_64-pc-windows-gnu.exe",
  "x86_64-pc-windows-msvc": "https://github.com/bazelbuild/rules_rust/releases/download/0.26.0/cargo-bazel-x86_64-pc-windows-msvc.exe",
  "x86_64-unknown-linux-gnu": "https://github.com/bazelbuild/rules_rust/releases/download/0.26.0/cargo-bazel-x86_64-unknown-linux-gnu",
  "x86_64-unknown-linux-musl": "https://github.com/bazelbuild/rules_rust/releases/download/0.26.0/cargo-bazel-x86_64-unknown-linux-musl"
}

# Example:
# {
#     "x86_64-unknown-linux-gnu": "1d687fcc860dc8a1aa6198e531f0aee0637ed506d6a412fe2b9884ff5b2b17c0",
#     "x86_64-apple-darwin": "0363e450125002f581d29cf632cc876225d738cfa433afa85ca557afb671eafa",
#     "x86_64-pc-windows-msvc": "f5647261d989f63dafb2c3cb8e131b225338a790386c06cf7112e43dd9805882",
# }
CARGO_BAZEL_SHA256S = {
  "aarch64-apple-darwin": "dfb4c75aab86cb5c6c71015e61d09ba45ad3a99ee6209eca8671bfe18e0ebe73",
  "aarch64-pc-windows-msvc": "f49949ee1faf2945efcb1dd4f377beaec6487b55f4f9f1e2cc2af3addc5e0868",
  "aarch64-unknown-linux-gnu": "d66b7ad9eb00c7982a8abcd6b86d95ac101952adf40213aa0ca7d9d278f2c6ee",
  "x86_64-apple-darwin": "4f50e6bd01c4ef7ad53ecb932c3ac8c167fc87f204690193d34c4f809ebbdd90",
  "x86_64-pc-windows-gnu": "06d129d807836dd8337e0e86fc02b359af89851499a53ecbf437256e4fff0fd0",
  "x86_64-pc-windows-msvc": "33ba74559e64a3766cdeede86c1c047fc877c63c3e71a87043f607b51dff6a5d",
  "x86_64-unknown-linux-gnu": "f8bd09f7fab9d926491e313def9de7a4b80466c2e890b3253257d732fdc52db4",
  "x86_64-unknown-linux-musl": "2a60f67c661170599badbc54346d9273cdcec239b14cc9bccb999099b0ee413d"
}

# Example:
# Label("//crate_universe:cargo_bazel_bin")
CARGO_BAZEL_LABEL = Label("//crate_universe:cargo_bazel_bin")
