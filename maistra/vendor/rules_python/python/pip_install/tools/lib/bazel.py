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

WHEEL_FILE_LABEL = "whl"
PY_LIBRARY_LABEL = "pkg"
DATA_LABEL = "data"
DIST_INFO_LABEL = "dist_info"
WHEEL_ENTRY_POINT_PREFIX = "rules_python_wheel_entry_point"


def sanitise_name(name: str, prefix: str) -> str:
    """Sanitises the name to be compatible with Bazel labels.

    There are certain requirements around Bazel labels that we need to consider. From the Bazel docs:

        Package names must be composed entirely of characters drawn from the set A-Z, a–z, 0–9, '/', '-', '.', and '_',
        and cannot start with a slash.

    Due to restrictions on Bazel labels we also cannot allow hyphens. See
    https://github.com/bazelbuild/bazel/issues/6841

    Further, rules-python automatically adds the repository root to the PYTHONPATH, meaning a package that has the same
    name as a module is picked up. We workaround this by prefixing with `pypi__`. Alternatively we could require
    `--noexperimental_python_import_all_repositories` be set, however this breaks rules_docker.
    See: https://github.com/bazelbuild/bazel/issues/2636
    """

    return prefix + name.replace("-", "_").replace(".", "_").lower()


def _whl_name_to_repo_root(whl_name: str, repo_prefix: str) -> str:
    return "@{}//".format(sanitise_name(whl_name, prefix=repo_prefix))


def sanitised_repo_library_label(whl_name: str, repo_prefix: str) -> str:
    return '"{}:{}"'.format(
        _whl_name_to_repo_root(whl_name, repo_prefix), PY_LIBRARY_LABEL
    )


def sanitised_repo_file_label(whl_name: str, repo_prefix: str) -> str:
    return '"{}:{}"'.format(
        _whl_name_to_repo_root(whl_name, repo_prefix), WHEEL_FILE_LABEL
    )
