This Java tools version was built from the bazel repository at commit hash 5576269cba82578ee4e80c362b39d86e87c25780
using bazel version 1.2.0.
To build from source the same zip run the commands:

$ git clone https://github.com/bazelbuild/bazel.git
$ git checkout 5576269cba82578ee4e80c362b39d86e87c25780
$ bazel build //src:java_tools_java11.zip
