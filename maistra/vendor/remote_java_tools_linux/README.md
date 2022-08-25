This Java tools version was built from the bazel repository at commit hash 40c9b756d7f8bac20801e2a54e3d83b2112c80c6
using bazel version 4.2.1.
To build from source the same zip run the commands:

$ git clone https://github.com/bazelbuild/bazel.git
$ git checkout 40c9b756d7f8bac20801e2a54e3d83b2112c80c6
$ bazel build //src:java_tools_prebuilt.zip
