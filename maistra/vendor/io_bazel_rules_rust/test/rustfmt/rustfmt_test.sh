#!/bin/bash
set -euxo pipefail

formatted="$(rlocation io_bazel_rules_rust/test/rustfmt/formatted.rs)"
unformatted="$(rlocation io_bazel_rules_rust/test/rustfmt/unformatted.rs)"

# Ensure that the file was formatted
! diff "$unformatted" "$formatted"
