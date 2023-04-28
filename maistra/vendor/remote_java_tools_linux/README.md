This Java tools version was built from the bazel repository at commit hash 7bd0ab63a8441c3f3d7f495d09ed2bed38762874
using bazel version 5.3.2 on platform linux.
To build from source the same zip run the commands:

$ git clone https://github.com/bazelbuild/bazel.git
$ git checkout 7bd0ab63a8441c3f3d7f495d09ed2bed38762874
$ bazel build //src:java_tools_prebuilt.zip
