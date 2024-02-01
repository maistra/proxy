# Copyright 2023 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import json
from argparse import ArgumentParser


def parse_common_args(parser: ArgumentParser) -> ArgumentParser:
    parser.add_argument(
        "--repo",
        action="store",
        required=True,
        help="The external repo name to install dependencies. In the format '@{REPO_NAME}'",
    )
    parser.add_argument(
        "--isolated",
        action="store_true",
        help="Whether or not to include the `--isolated` pip flag.",
    )
    parser.add_argument(
        "--extra_pip_args",
        action="store",
        help="Extra arguments to pass down to pip.",
    )
    parser.add_argument(
        "--pip_data_exclude",
        action="store",
        help="Additional data exclusion parameters to add to the pip packages BUILD file.",
    )
    parser.add_argument(
        "--enable_implicit_namespace_pkgs",
        action="store_true",
        help="Disables conversion of implicit namespace packages into pkg-util style packages.",
    )
    parser.add_argument(
        "--environment",
        action="store",
        help="Extra environment variables to set on the pip environment.",
    )
    parser.add_argument(
        "--repo-prefix",
        required=True,
        help="Prefix to prepend to packages",
    )
    parser.add_argument(
        "--download_only",
        action="store_true",
        help="Use 'pip download' instead of 'pip wheel'. Disables building wheels from source, but allows use of "
        "--platform, --python-version, --implementation, and --abi in --extra_pip_args.",
    )
    return parser


def deserialize_structured_args(args):
    """Deserialize structured arguments passed from the starlark rules.
    Args:
        args: dict of parsed command line arguments
    """
    structured_args = ("extra_pip_args", "pip_data_exclude", "environment")
    for arg_name in structured_args:
        if args.get(arg_name) is not None:
            args[arg_name] = json.loads(args[arg_name])["arg"]
        else:
            args[arg_name] = []
    return args
