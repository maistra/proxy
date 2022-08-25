#!/usr/bin/env python3

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

import unittest
import csv
import io
import os

from rules_python.python.runfiles import runfiles
from tests.rpm import rpm_util

EXPECTED_RPM_MANIFEST_CSV = """
path,digest,user,group,mode,fflags,symlink
/test_dir/a,dc35e5df50b75e25610e1aaaa29edaa4,root,root,100644,,
/test_dir/b,dc35e5df50b75e25610e1aaaa29edaa4,root,root,100644,,
/test_dir/subdir/c,dc35e5df50b75e25610e1aaaa29edaa4,root,root,100644,,
/test_dir/subdir/d,dc35e5df50b75e25610e1aaaa29edaa4,root,root,100644,,
/test_dir/e,d41d8cd98f00b204e9800998ecf8427e,root,root,100644,,
/test_dir/f,d41d8cd98f00b204e9800998ecf8427e,root,root,100644,,
""".strip()


class PkgRpmCompManifest(unittest.TestCase):
    def setUp(self):
        self.runfiles = runfiles.Create()
        self.maxDiff = None
        # Allow for parameterization of this test based on the desired RPM to
        # test.

        # runfiles prefers Bazely (POSIX-style) relative paths, so we can't
        # really use os.path.join() here.
        self.rpm_path = self.runfiles.Rlocation('/'.join([
            os.environ["TEST_WORKSPACE"],
            "tests", "rpm", "tree_artifacts",
            # The object behind os.environ is not a dict, and thus doesn't have
            # the "getdefault()" we'd otherwise use here.
            os.environ["TEST_RPM"] if "TEST_RPM" in os.environ else "test_dirs_rpm.rpm",
        ]))

    def test_contents_match(self):
        sio = io.StringIO(EXPECTED_RPM_MANIFEST_CSV)
        manifest_reader = csv.DictReader(sio)
        manifest_specs = {r['path']: r for r in manifest_reader}

        rpm_specs = rpm_util.read_rpm_filedata(self.rpm_path)

        self.assertDictEqual(manifest_specs, rpm_specs)


if __name__ == "__main__":
    unittest.main()
