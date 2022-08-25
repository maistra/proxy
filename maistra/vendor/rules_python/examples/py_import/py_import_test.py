# Copyright 2017-2019 The Bazel Authors. All rights reserved.
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

from examples.py_import import helloworld


class HelloWorldTest(unittest.TestCase):
    def test_helloworld(self):
        hw = helloworld.HelloWorld()
        hw.SayHello()

    def test_helloworld_async(self):
        hw = helloworld.HelloWorld()
        hw.SayHelloAsync()
        hw.Stop()

    def test_helloworld_multiple(self):
        hw = helloworld.HelloWorld()
        hw.SayHelloAsync()
        hw.SayHelloAsync()
        hw.SayHelloAsync()
        hw.SayHelloAsync()
        hw.Stop()


if __name__ == "__main__":
    unittest.main()
