This Java tools version was built from the bazel repository at commit hash 239b2aab17cc1f007b2221ada9074bbe0c58db88
using bazel version 3.3.0.
To build from source the same zip run the commands:

$ git clone https://github.com/bazelbuild/bazel.git
$ git checkout 239b2aab17cc1f007b2221ada9074bbe0c58db88
$ bazel build //src:java_tools_java11.zip
