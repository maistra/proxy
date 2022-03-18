Go workspace rules
==================

.. Links to other sites and pages
.. _gazelle: tools/gazelle/README.rst
.. _github.com/bazelbuild/bazel-skylib: https://github.com/bazelbuild/bazel-skylib
.. _github.com/gogo/protobuf: https://github.com/gogo/protobuf
.. _github.com/golang/protobuf: https://github.com/golang/protobuf/
.. _github.com/google/protobuf: https://github.com/google/protobuf/
.. _github.com/googleapis/googleapis: https://github.com/googleapis/googleapis
.. _github.com/mwitkow/go-proto-validators: https://github.com/mwitkow/go-proto-validators
.. _golang.org/x/net: https://github.com/golang/net/
.. _golang.org/x/sys: https://github.com/golang/sys/
.. _golang.org/x/text: https://github.com/golang/text/
.. _golang.org/x/tools: https://github.com/golang/tools/
.. _google.golang.org/genproto: https://github.com/google/go-genproto
.. _google.golang.org/grpc: https://github.com/grpc/grpc-go
.. _http_archive: https://github.com/bazelbuild/bazel/blob/master/tools/build_defs/repo/http.bzl
.. _nested workspaces: https://bazel.build/designs/2016/09/19/recursive-ws-parsing.html
.. _nogo: nogo.rst#nogo
.. _normal go logic: https://golang.org/cmd/go/#hdr-Remote_import_paths
.. _repositories.bzl: https://github.com/bazelbuild/rules_go/blob/master/go/private/repositories.bzl
.. _rules_proto: https://github.com/bazelbuild/rules_proto
.. _third_party: https://github.com/bazelbuild/rules_go/tree/master/third_party
.. _toolchains: toolchains.rst

.. Go rules
.. _go_library: core.rst#go_library
.. _go_proto_library: https://github.com/bazelbuild/rules_go/blob/master/proto/core.rst#go-proto-library
.. _go_register_toolchains: toolchains.rst#go_register_toolchains
.. _go_repository: https://github.com/bazelbuild/bazel-gazelle/blob/master/repository.rst#go_repository
.. _go_toolchain: toolchains.rst#go_toolchain

.. Other rules
.. _git_repository: https://github.com/bazelbuild/bazel/blob/master/tools/build_defs/repo/git.bzl
.. _proto_library: https://github.com/bazelbuild/rules_proto

.. Issues
.. _#1986: https://github.com/bazelbuild/rules_go/issues/1986

.. role:: param(kbd)
.. role:: type(emphasis)
.. role:: value(code)
.. |mandatory| replace:: **mandatory value**

This document describes workspace rules, functions, and dependencies intended
to be used in the ``WORKSPACE`` file.a

See also the `toolchains`_ for information on `go_register_toolchains`_ and 
other rules used to download and register toolchains.

Contents
--------

* `go_rules_dependencies`_
* `Proto dependencies`_
* `gRPC dependencies`_
* `Overriding dependencies`_


go_rules_dependencies
---------------------

``go_rules_dependencies`` is a function that registers external dependencies
needed by the Go rules. Projects that use rules_go should *always* call it from
WORKSPACE. It may be called before or after other workspace rules.

See `Overriding dependencies`_ for instructions on using a different version
of one of the repositories below.

``go_rules_dependencies`` declares the repositories in the table below.
It also declares some internal repositories not described here.

+-------------------------------------------------+-------------------------------------------+
| **Name**                                        | **Path**                                  |
+-------------------------------------------------+-------------------------------------------+
| :value:`bazel_skylib`                           | `github.com/bazelbuild/bazel-skylib`_     |
+-------------------------------------------------+-------------------------------------------+
| A library of useful Starlark functions, used in the implementation                          |
| of rules_go.                                                                                |
+-------------------------------------------------+-------------------------------------------+
| :value:`org_golang_x_tools`                     | `golang.org/x/tools`_                     |
+-------------------------------------------------+-------------------------------------------+
| The Go tools module. Provides the analysis framework that nogo_ is based on.                |
| Also provides other package loading and testing infrastructure.                             |
+-------------------------------------------------+-------------------------------------------+
| :value:`com_github_golang_protobuf`             | `github.com/golang/protobuf`_             |
+-------------------------------------------------+-------------------------------------------+
| The Go protobuf plugin and runtime. When overriding this, make sure to use                  |
| ``@io_bazel_rules_go//third_party:com_github_golang_protobuf-extras.patch``.                |
| This is needed to support both pre-generated and dynamically generated                      |
| proto libraries.                                                                            |
+-------------------------------------------------+-------------------------------------------+
| :value:`com_github_mwitkow_go_proto_validators` | `github.com/mwitkow/go-proto-validators`_ |
+-------------------------------------------------+-------------------------------------------+
| Legacy definition for proto plugin. Ideally ``go_rules_dependencies`` should                |
| not provide this.                                                                           |
+-------------------------------------------------+-------------------------------------------+
| :value:`com_github_gogo_protobuf`               | `github.com/gogo/protobuf`_               |
+-------------------------------------------------+-------------------------------------------+
| Legacy definition for proto plugins. Ideally ``go_rules_dependencies`` should               |
| not provide this.                                                                           |
+-------------------------------------------------+-------------------------------------------+
| :value:`org_golang_google_genproto`             | `google.golang.org/genproto`_             |
+-------------------------------------------------+-------------------------------------------+
| Pre-generated proto libraries for gRPC and Google APIs. Ideally,                            |
| ``go_rules_dependencies`` should provide this, but it doesn't change often,                 |
| and many things break without it.                                                           |
+-------------------------------------------------+-------------------------------------------+
| :value:`go_googleapis`                          | `github.com/googleapis/googleapis`_       |
+-------------------------------------------------+-------------------------------------------+
| Like :value:`org_golang_google_genproto` but provides ``go_proto_library``                  |
| targets instead of ``go_library``. Ideally we should use                                    |
| ``com_google_googleapis``, but Gazelle still resolves imports to this repo.                 |
| See `#1986`_.                                                                               |
+-------------------------------------------------+-------------------------------------------+

Proto dependencies
------------------

In order to build `proto_library`_ and `go_proto_library`_ rules, you must
add a dependency on ``com_google_protobuf`` (perhaps on a newer version)
in order to build the ``protoc`` compiler. You'll need a C/C++ toolchain for
the execution platform, too.

.. code:: bzl

    load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

    http_archive(
        name = "com_google_protobuf",
        sha256 = "9748c0d90e54ea09e5e75fb7fac16edce15d2028d4356f32211cfa3c0e956564",
        strip_prefix = "protobuf-3.11.4",
        urls = ["https://github.com/protocolbuffers/protobuf/archive/v3.11.4.zip"],
    )

    load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

    protobuf_deps()

The `proto_library`_ rule is provided by the `rules_proto`_
repository. ``protoc-gen-go``, the Go proto compiler plugin, is provided by the
repository ``com_github_golang_protobuf``. Both are declared by
`go_rules_dependencies`_  by default. You won't need to declare an
explicit dependency unless you specifically want to use a different version. See
`Overriding dependencies`_ for instructions on using a different version.

gRPC dependencies
-----------------

In order to build ``go_proto_library`` rules with the gRPC plugin,
several additional dependencies are needed. At minimum, you'll need to
declare ``org_golang_google_grpc``, ``org_golang_x_net``, and
``org_golang_x_text``.

If you're using Gazelle, and you already import ``google.golang.org/grpc``
from a .go file somewhere in your repository, and you're also using Go modules
to manage dependencies, you can generate these rules with
``bazel run //:gazelle -- update-repos -from_file=go.mod``.

Make sure you set ``build_file_proto_mode = "disable"`` on the
`go_repository`_ rule for ``org_golang_google_grpc``.

For example:

.. code:: bzl

    load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies", "go_repository")

    gazelle_dependencies()

    go_repository(
        name = "org_golang_google_grpc",
        build_file_proto_mode = "disable",
        importpath = "google.golang.org/grpc",
        sum = "h1:J0UbZOIrCAl+fpTOf8YLs4dJo8L/owV4LYVtAXQoPkw=",
        version = "v1.22.0",
    )

    go_repository(
        name = "org_golang_x_net",
        importpath = "golang.org/x/net",
        sum = "h1:oWX7TPOiFAMXLq8o0ikBYfCJVlRHBcsciT5bXOrH628=",
        version = "v0.0.0-20190311183353-d8887717615a",
    )

    go_repository(
        name = "org_golang_x_text",
        importpath = "golang.org/x/text",
        sum = "h1:g61tztE5qeGQ89tm6NTjjM9VPIm088od1l6aSorWRWg=",
        version = "v0.3.0",
    )

Overriding dependencies
-----------------------

You can override a dependency declared in ``go_rules_dependencies`` by
declaring a repository rule in WORKSPACE with the same name *before* the call
to ``go_rules_dependencies``.

For example, this is how you would override ``com_github_golang_protobuf``:

.. code:: bzl

    load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
    load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

    http_archive(
        name = "io_bazel_rules_go",
        sha256 = "7b9bbe3ea1fccb46dcfa6c3f3e29ba7ec740d8733370e21cdc8937467b4a4349",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v0.22.4/rules_go-v0.22.4.tar.gz",
            "https://github.com/bazelbuild/rules_go/releases/download/v0.22.4/rules_go-v0.22.4.tar.gz",
        ],
    )

    http_archive(
        name = "bazel_gazelle",
        sha256 = "d8c45ee70ec39a57e7a05e5027c32b1576cc7f16d9dd37135b0eddde45cf1b10",
        urls = [
            "https://storage.googleapis.com/bazel-mirror/github.com/bazelbuild/bazel-gazelle/releases/download/v0.20.0/bazel-gazelle-v0.20.0.tar.gz",
            "https://github.com/bazelbuild/bazel-gazelle/releases/download/v0.20.0/bazel-gazelle-v0.20.0.tar.gz",
        ],
    )

    http_archive(
        name = "com_google_protobuf",
        sha256 = "9748c0d90e54ea09e5e75fb7fac16edce15d2028d4356f32211cfa3c0e956564",
        strip_prefix = "protobuf-3.11.4",
        urls = ["https://github.com/protocolbuffers/protobuf/archive/v3.11.4.zip"],
    )

    load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
    load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies", "go_repository")
    load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

    go_repository(
        name = "com_github_golang_protobuf",
        build_file_proto_mode = "disable_global",
        importpath = "github.com/golang/protobuf",
        patch_args = ["-p1"],
        patches = ["@io_bazel_rules_go//third_party:com_github_golang_protobuf-extras.patch"],
        sum = "h1:F768QJ1E9tib+q5Sc8MkdJi1RxLTbRcTf8LJV56aRls=",
        version = "v1.3.5",
    )

    go_rules_dependencies()

    go_register_toolchains()

    gazelle_dependencies()

    protobuf_deps()

Some of the dependencies declared by ``go_rules_dependencies`` require
additional patches and or adjustments compared to what `go_repository`_
generates by default (as ``com_github_golang_protobuf`` does in the example
above). Patches may be found in the `third_party`_ directory.
See notes in `repositories.bzl`_. If you're generated build files with
`go_repository`_, you do not need the ``*-gazelle.patch`` files.
