This Java tools version was built from the bazel repository at commit hash 159c3fe59d3cb2be2ec882911ab27384c25c0618
using bazel version 3.5.0.
To build from source the same zip run the commands:

$ git clone https://github.com/bazelbuild/bazel.git
$ git checkout 159c3fe59d3cb2be2ec882911ab27384c25c0618
$ bazel build //src:java_tools_java11.zip
