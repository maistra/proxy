# Copyright 2018 The Bazel Authors. All rights reserved.
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

import os
import platform
import unittest
import zipfile


class WheelTest(unittest.TestCase):
    def test_py_library_wheel(self):
        filename = os.path.join(
            os.environ["TEST_SRCDIR"],
            "rules_python",
            "examples",
            "wheel",
            "example_minimal_library-0.0.1-py3-none-any.whl",
        )
        with zipfile.ZipFile(filename) as zf:
            self.assertEquals(
                zf.namelist(),
                [
                    "examples/wheel/lib/module_with_data.py",
                    "examples/wheel/lib/simple_module.py",
                    "example_minimal_library-0.0.1.dist-info/WHEEL",
                    "example_minimal_library-0.0.1.dist-info/METADATA",
                    "example_minimal_library-0.0.1.dist-info/RECORD",
                ],
            )

    def test_py_package_wheel(self):
        filename = os.path.join(
            os.environ["TEST_SRCDIR"],
            "rules_python",
            "examples",
            "wheel",
            "example_minimal_package-0.0.1-py3-none-any.whl",
        )
        with zipfile.ZipFile(filename) as zf:
            self.assertEquals(
                zf.namelist(),
                [
                    "examples/wheel/lib/data.txt",
                    "examples/wheel/lib/module_with_data.py",
                    "examples/wheel/lib/simple_module.py",
                    "examples/wheel/main.py",
                    "example_minimal_package-0.0.1.dist-info/WHEEL",
                    "example_minimal_package-0.0.1.dist-info/METADATA",
                    "example_minimal_package-0.0.1.dist-info/RECORD",
                ],
            )

    def test_customized_wheel(self):
        filename = os.path.join(
            os.environ["TEST_SRCDIR"],
            "rules_python",
            "examples",
            "wheel",
            "example_customized-0.0.1-py3-none-any.whl",
        )
        with zipfile.ZipFile(filename) as zf:
            self.assertEquals(
                zf.namelist(),
                [
                    "examples/wheel/lib/data.txt",
                    "examples/wheel/lib/module_with_data.py",
                    "examples/wheel/lib/simple_module.py",
                    "examples/wheel/main.py",
                    "example_customized-0.0.1.dist-info/WHEEL",
                    "example_customized-0.0.1.dist-info/METADATA",
                    "example_customized-0.0.1.dist-info/entry_points.txt",
                    "example_customized-0.0.1.dist-info/RECORD",
                ],
            )
            record_contents = zf.read("example_customized-0.0.1.dist-info/RECORD")
            wheel_contents = zf.read("example_customized-0.0.1.dist-info/WHEEL")
            metadata_contents = zf.read("example_customized-0.0.1.dist-info/METADATA")
            entry_point_contents = zf.read(
                "example_customized-0.0.1.dist-info/entry_points.txt"
            )
            # The entries are guaranteed to be sorted.
            if platform.system() == "Windows":
                self.assertEquals(
                    record_contents,
                    b"""\
example_customized-0.0.1.dist-info/METADATA,sha256=pzE96o3Sp63TDzxAZgl0F42EFevm8x15vpDLqDVp_EQ,378
example_customized-0.0.1.dist-info/RECORD,,
example_customized-0.0.1.dist-info/WHEEL,sha256=sobxWSyDDkdg_rinUth-jxhXHqoNqlmNMJY3aTZn2Us,91
example_customized-0.0.1.dist-info/entry_points.txt,sha256=pqzpbQ8MMorrJ3Jp0ntmpZcuvfByyqzMXXi2UujuXD0,137
examples/wheel/lib/data.txt,sha256=9vJKEdfLu8bZRArKLroPZJh1XKkK3qFMXiM79MBL2Sg,12
examples/wheel/lib/module_with_data.py,sha256=8s0Khhcqz3yVsBKv2IB5u4l4TMKh7-c_V6p65WVHPms,637
examples/wheel/lib/simple_module.py,sha256=z2hwciab_XPNIBNH8B1Q5fYgnJvQTeYf0ZQJpY8yLLY,637
examples/wheel/main.py,sha256=sgg5iWN_9inYBjm6_Zw27hYdmo-l24fA-2rfphT-IlY,909
""",
                )
            else:
                self.assertEquals(
                    record_contents,
                    b"""\
example_customized-0.0.1.dist-info/METADATA,sha256=TeeEmokHE2NWjkaMcVJuSAq4_AXUoIad2-SLuquRmbg,372
example_customized-0.0.1.dist-info/RECORD,,
example_customized-0.0.1.dist-info/WHEEL,sha256=sobxWSyDDkdg_rinUth-jxhXHqoNqlmNMJY3aTZn2Us,91
example_customized-0.0.1.dist-info/entry_points.txt,sha256=pqzpbQ8MMorrJ3Jp0ntmpZcuvfByyqzMXXi2UujuXD0,137
examples/wheel/lib/data.txt,sha256=9vJKEdfLu8bZRArKLroPZJh1XKkK3qFMXiM79MBL2Sg,12
examples/wheel/lib/module_with_data.py,sha256=8s0Khhcqz3yVsBKv2IB5u4l4TMKh7-c_V6p65WVHPms,637
examples/wheel/lib/simple_module.py,sha256=z2hwciab_XPNIBNH8B1Q5fYgnJvQTeYf0ZQJpY8yLLY,637
examples/wheel/main.py,sha256=sgg5iWN_9inYBjm6_Zw27hYdmo-l24fA-2rfphT-IlY,909
""",
                )
            self.assertEquals(
                wheel_contents,
                b"""\
Wheel-Version: 1.0
Generator: bazel-wheelmaker 1.0
Root-Is-Purelib: true
Tag: py3-none-any
""",
            )
            if platform.system() == "Windows":
                self.assertEquals(
                    metadata_contents,
                    b"""\
Metadata-Version: 2.1
Name: example_customized
Version: 0.0.1
Author: Example Author with non-ascii characters: \xc3\x85\xc2\xbc\xc3\x83\xc2\xb3\xc3\x85\xc2\x82w
Author-email: example@example.com
Home-page: www.example.com
License: Apache 2.0
Classifier: License :: OSI Approved :: Apache Software License
Classifier: Intended Audience :: Developers
Requires-Dist: pytest

This is a sample description of a wheel.
""",
                )
            else:
                self.assertEquals(
                    metadata_contents,
                    b"""\
Metadata-Version: 2.1
Name: example_customized
Version: 0.0.1
Author: Example Author with non-ascii characters: \xc5\xbc\xc3\xb3\xc5\x82w
Author-email: example@example.com
Home-page: www.example.com
License: Apache 2.0
Classifier: License :: OSI Approved :: Apache Software License
Classifier: Intended Audience :: Developers
Requires-Dist: pytest

This is a sample description of a wheel.
""",
                )
            self.assertEquals(
                entry_point_contents,
                b"""\
[console_scripts]
another = foo.bar:baz
customized_wheel = examples.wheel.main:main

[group2]
first = first.main:f
second = second.main:s""",
            )

    def test_filename_escaping(self):
        filename = os.path.join(
            os.environ["TEST_SRCDIR"],
            "rules_python",
            "examples",
            "wheel",
            "file_name_escaping-0.0.1_r7-py3-none-any.whl",
        )
        with zipfile.ZipFile(filename) as zf:
            self.assertEquals(
                zf.namelist(),
                [
                    "examples/wheel/lib/data.txt",
                    "examples/wheel/lib/module_with_data.py",
                    "examples/wheel/lib/simple_module.py",
                    "examples/wheel/main.py",
                    # PEP calls for replacing only in the archive filename.
                    # Alas setuptools also escapes in the dist-info directory
                    # name, so let's be compatible.
                    "file_name_escaping-0.0.1_r7.dist-info/WHEEL",
                    "file_name_escaping-0.0.1_r7.dist-info/METADATA",
                    "file_name_escaping-0.0.1_r7.dist-info/RECORD",
                ],
            )
            metadata_contents = zf.read(
                "file_name_escaping-0.0.1_r7.dist-info/METADATA"
            )
            self.assertEquals(
                metadata_contents,
                b"""\
Metadata-Version: 2.1
Name: file~~name-escaping
Version: 0.0.1-r7

UNKNOWN
""",
            )

    def test_custom_package_root_wheel(self):
        filename = os.path.join(
            os.environ["TEST_SRCDIR"],
            "rules_python",
            "examples",
            "wheel",
            "example_custom_package_root-0.0.1-py3-none-any.whl",
        )

        with zipfile.ZipFile(filename) as zf:
            self.assertEquals(
                zf.namelist(),
                [
                    "wheel/lib/data.txt",
                    "wheel/lib/module_with_data.py",
                    "wheel/lib/simple_module.py",
                    "wheel/main.py",
                    "example_custom_package_root-0.0.1.dist-info/WHEEL",
                    "example_custom_package_root-0.0.1.dist-info/METADATA",
                    "example_custom_package_root-0.0.1.dist-info/RECORD",
                ],
            )

    def test_custom_package_root_multi_prefix_wheel(self):
        filename = os.path.join(
            os.environ["TEST_SRCDIR"],
            "rules_python",
            "examples",
            "wheel",
            "example_custom_package_root_multi_prefix-0.0.1-py3-none-any.whl",
        )

        with zipfile.ZipFile(filename) as zf:
            self.assertEquals(
                zf.namelist(),
                [
                    "data.txt",
                    "module_with_data.py",
                    "simple_module.py",
                    "main.py",
                    "example_custom_package_root_multi_prefix-0.0.1.dist-info/WHEEL",
                    "example_custom_package_root_multi_prefix-0.0.1.dist-info/METADATA",
                    "example_custom_package_root_multi_prefix-0.0.1.dist-info/RECORD",
                ],
            )

    def test_custom_package_root_multi_prefix_reverse_order_wheel(self):
        filename = os.path.join(
            os.environ["TEST_SRCDIR"],
            "rules_python",
            "examples",
            "wheel",
            "example_custom_package_root_multi_prefix_reverse_order-0.0.1-py3-none-any.whl",
        )

        with zipfile.ZipFile(filename) as zf:
            self.assertEquals(
                zf.namelist(),
                [
                    "lib/data.txt",
                    "lib/module_with_data.py",
                    "lib/simple_module.py",
                    "main.py",
                    "example_custom_package_root_multi_prefix_reverse_order-0.0.1.dist-info/WHEEL",
                    "example_custom_package_root_multi_prefix_reverse_order-0.0.1.dist-info/METADATA",
                    "example_custom_package_root_multi_prefix_reverse_order-0.0.1.dist-info/RECORD",
                ],
            )

    def test_python_requires_wheel(self):
        filename = os.path.join(
            os.environ["TEST_SRCDIR"],
            "rules_python",
            "examples",
            "wheel",
            "example_python_requires_in_a_package-0.0.1-py3-none-any.whl",
        )
        with zipfile.ZipFile(filename) as zf:
            metadata_contents = zf.read(
                "example_python_requires_in_a_package-0.0.1.dist-info/METADATA"
            )
            # The entries are guaranteed to be sorted.
            self.assertEquals(
                metadata_contents,
                b"""\
Metadata-Version: 2.1
Name: example_python_requires_in_a_package
Version: 0.0.1
Requires-Python: >=2.7, !=3.0.*, !=3.1.*, !=3.2.*, !=3.3.*, !=3.4.*

UNKNOWN
""",
            )

    def test_python_abi3_binary_wheel(self):
        filename = os.path.join(
            os.environ["TEST_SRCDIR"],
            "rules_python",
            "examples",
            "wheel",
            "example_python_abi3_binary_wheel-0.0.1-cp38-abi3-manylinux2014_x86_64.whl",
        )
        with zipfile.ZipFile(filename) as zf:
            metadata_contents = zf.read(
                "example_python_abi3_binary_wheel-0.0.1.dist-info/METADATA"
            )
            # The entries are guaranteed to be sorted.
            self.assertEqual(
                metadata_contents,
                b"""\
Metadata-Version: 2.1
Name: example_python_abi3_binary_wheel
Version: 0.0.1
Requires-Python: >=3.8

UNKNOWN
""",
            )
            wheel_contents = zf.read(
                "example_python_abi3_binary_wheel-0.0.1.dist-info/WHEEL"
            )
            self.assertEqual(
                wheel_contents,
                b"""\
Wheel-Version: 1.0
Generator: bazel-wheelmaker 1.0
Root-Is-Purelib: false
Tag: cp38-abi3-manylinux2014_x86_64
""",
            )

    def test_genrule_creates_directory_and_is_included_in_wheel(self):
        filename = os.path.join(
            os.environ["TEST_SRCDIR"],
            "rules_python",
            "examples",
            "wheel",
            "use_genrule_with_dir_in_outs-0.0.1-py3-none-any.whl",
        )

        with zipfile.ZipFile(filename) as zf:
            self.assertEquals(
                zf.namelist(),
                [
                    "examples/wheel/main.py",
                    "examples/wheel/someDir/foo.py",
                    "use_genrule_with_dir_in_outs-0.0.1.dist-info/WHEEL",
                    "use_genrule_with_dir_in_outs-0.0.1.dist-info/METADATA",
                    "use_genrule_with_dir_in_outs-0.0.1.dist-info/RECORD",
                ],
            )


if __name__ == "__main__":
    unittest.main()
