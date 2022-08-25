# Copyright 2021 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""A resilient API layer wrapping compilation and other logic for Rust rules.

This module is meant to be used by custom rules that need to compile Rust code
and cannot simply rely on writing a macro that wraps `rust_library`. This module
provides the lower-level interface to Rust providers, actions, and functions.
Do not load this file directly; instead, load the top-level `defs.bzl` file,
which exports the `rust_common` struct.

In the Bazel lingo, `rust_common` gives the access to the Rust Sandwich API.
"""

load(":providers.bzl", "CrateInfo", "DepInfo", "StdLibInfo")

# These constants only represent the default value for attributes and macros
# defiend in `rules_rust`. Like any attribute public attribute, they can be
# overwritten by the user on the rules they're defiend on.
#
# Note: Code in `.github/workflows/crate_universe.yaml` looks for this line, if
# you remove it or change its format, you will also need to update that code.
DEFAULT_RUST_VERSION = "1.59.0"
DEFAULT_RUST_EDITION = "2018"

def _create_crate_info(**kwargs):
    """A constructor for a `CrateInfo` provider

    This function should be used in place of directly creating a `CrateInfo`
    provider to improve API stability.

    Args:
        **kwargs: An inital set of keyword arguments.

    Returns:
        CrateInfo: A provider
    """
    if not "wrapped_crate_type" in kwargs:
        kwargs.update({"wrapped_crate_type": None})
    return CrateInfo(**kwargs)

rust_common = struct(
    create_crate_info = _create_crate_info,
    crate_info = CrateInfo,
    dep_info = DepInfo,
    stdlib_info = StdLibInfo,
    default_edition = DEFAULT_RUST_EDITION,
    default_version = DEFAULT_RUST_VERSION,
)
