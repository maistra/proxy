This Java tools version was built from the bazel repository at commit hash 342a4076783a703dbd33b67f1f292ef86f0772c2
using bazel version 5.2.0 on platform linux.
To build from source the same zip run the commands:

$ git clone https://github.com/bazelbuild/bazel.git
$ git checkout 342a4076783a703dbd33b67f1f292ef86f0772c2
$ bazel build //src:java_tools_prebuilt.zip
