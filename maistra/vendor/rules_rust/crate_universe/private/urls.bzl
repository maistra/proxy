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
  "x86_64-pc-windows-gnu": "https://github.com/bazelbuild/rules_rust/releases/download/0.2.1/cargo-bazel-x86_64-pc-windows-gnu.exe",
  "x86_64-unknown-linux-gnu": "https://github.com/bazelbuild/rules_rust/releases/download/0.2.1/cargo-bazel-x86_64-unknown-linux-gnu",
  "aarch64-apple-darwin": "https://github.com/bazelbuild/rules_rust/releases/download/0.2.1/cargo-bazel-aarch64-apple-darwin",
  "x86_64-pc-windows-msvc": "https://github.com/bazelbuild/rules_rust/releases/download/0.2.1/cargo-bazel-x86_64-pc-windows-msvc.exe",
  "x86_64-unknown-linux-musl": "https://github.com/bazelbuild/rules_rust/releases/download/0.2.1/cargo-bazel-x86_64-unknown-linux-musl",
  "x86_64-apple-darwin": "https://github.com/bazelbuild/rules_rust/releases/download/0.2.1/cargo-bazel-x86_64-apple-darwin",
  "aarch64-unknown-linux-gnu": "https://github.com/bazelbuild/rules_rust/releases/download/0.2.1/cargo-bazel-aarch64-unknown-linux-gnu"
}

# Example:
# {
#     "x86_64-unknown-linux-gnu": "1d687fcc860dc8a1aa6198e531f0aee0637ed506d6a412fe2b9884ff5b2b17c0",
#     "x86_64-apple-darwin": "0363e450125002f581d29cf632cc876225d738cfa433afa85ca557afb671eafa",
#     "x86_64-pc-windows-msvc": "f5647261d989f63dafb2c3cb8e131b225338a790386c06cf7112e43dd9805882",
# }
CARGO_BAZEL_SHA256S = {
  "aarch64-unknown-linux-gnu": "02e70cb3d2f1c76bc6450359bab5dd4b347fa4f958b74b0c9f6edfad75dbd332",
  "x86_64-apple-darwin": "66d03170c6c20f3566926fdcc00472281f1d3f1752fb95a44506e7b8ed19295c",
  "x86_64-pc-windows-msvc": "91fbc436fb32f603441b2b6cc8ea3e88c8c7146f823959c98a3d6091ae3de4c9",
  "x86_64-unknown-linux-musl": "1b1952c9f66538d04d0771fcf27ed11903420730984a9c5f2b56e33159c8f7c6",
  "x86_64-pc-windows-gnu": "d08bb060933b776fae48c4550ab21bbde484a9a43022ecd8c3a65c62a8b21f69",
  "x86_64-unknown-linux-gnu": "830e311b5eae23388b16304edf8cec9bcef53e133d38fdeb8b64a8b5c9a44961",
  "aarch64-apple-darwin": "6a706bf5a13484ea96ec13e5101a7f793dc4c70e130b7df4b2621118abbbb41d"
}

# Example:
# Label("//crate_universe:cargo_bazel_bin")
CARGO_BAZEL_LABEL = Label("//crate_universe:cargo_bazel_bin")
