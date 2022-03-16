This Java tools version was built from the bazel repository at commit hash 75f5d65bd7f19ef6b24967fcd3891e2ac342e65b
using bazel version 4.0.0.
To build from source the same zip run the commands:

$ git clone https://github.com/bazelbuild/bazel.git
$ git checkout 75f5d65bd7f19ef6b24967fcd3891e2ac342e65b
$ bazel build //src:java_tools_java11.zip
