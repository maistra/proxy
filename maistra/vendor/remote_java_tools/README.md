This Java tools version was built from the bazel repository at commit hash bd91cb131785a6693d45c7840a229d345e48d40b
using bazel version 6.3.0.
To build from source the same zip run the commands:

$ git clone https://github.com/bazelbuild/bazel.git
$ git checkout bd91cb131785a6693d45c7840a229d345e48d40b
$ bazel build //src:java_tools.zip
