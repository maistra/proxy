# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""LicensePolicyProvider."""

LicensePolicyInfo = provider(
    doc = """Declares a policy name and the license conditions allowable under it.""",
    fields = {
        "name": "License Policy Name",
        "label": "The full path to the license policy definition.",
        "conditions": "List of conditions to be met when using this software.",
    },
)
