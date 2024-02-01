Go rules for Bazel_
=====================

.. Links to external sites and pages
.. _//tests/core/cross: https://github.com/bazelbuild/rules_go/blob/master/tests/core/cross/BUILD.bazel
.. _Avoiding conflicts: proto/core.rst#avoiding-conflicts
.. _Bazel labels: https://docs.bazel.build/versions/master/build-ref.html#labels
.. _Bazel: https://bazel.build/
.. _Build modes: go/modes.rst
.. _Bzlmod: https://bazel.build/external/overview#bzlmod
.. _Go with Bzlmod: docs/go/core/bzlmod.md
.. _Core rules: docs/go/core/rules.md
.. _Coverage: https://bazel.google.cn/docs/coverage
.. _Dependencies: go/dependencies.rst
.. _Deprecation schedule: https://github.com/bazelbuild/rules_go/wiki/Deprecation-schedule
.. _Gopher Slack: https://invite.slack.golangbridge.org/
.. _gopls integration: https://github.com/bazelbuild/rules_go/wiki/Editor-setup
.. _Overriding dependencies: go/dependencies.rst#overriding-dependencies
.. _Proto dependencies: go/dependencies.rst#proto-dependencies
.. _Proto rules: proto/core.rst
.. _Protocol buffers: proto/core.rst
.. _Running Bazel Tests on Travis CI: https://kev.inburke.com/kevin/bazel-tests-on-travis-ci/
.. _Toolchains: go/toolchains.rst
.. _Using rules_go on Windows: windows.rst
.. _bazel-go-discuss: https://groups.google.com/forum/#!forum/bazel-go-discuss
.. _configuration transition: https://docs.bazel.build/versions/master/skylark/lib/transition.html
.. _gRPC dependencies: go/dependencies.rst#grpc-dependencies
.. _gazelle update-repos: https://github.com/bazelbuild/bazel-gazelle#update-repos
.. _gazelle: https://github.com/bazelbuild/bazel-gazelle
.. _github.com/bazelbuild/bazel-gazelle: https://github.com/bazelbuild/bazel-gazelle
.. _github.com/bazelbuild/rules_go/go/tools/bazel: https://pkg.go.dev/github.com/bazelbuild/rules_go/go/tools/bazel?tab=doc
.. _korfuri/bazel-travis Use Bazel with Travis CI: https://github.com/korfuri/bazel-travis
.. _nogo build-time static analysis: go/nogo.rst
.. _nogo: go/nogo.rst
.. _rules_go and Gazelle roadmap: https://github.com/bazelbuild/rules_go/wiki/Roadmap
.. _#bazel on Go Slack: https://gophers.slack.com/archives/C1SCQE54N
.. _#go on Bazel Slack: https://bazelbuild.slack.com/archives/CDBP88Z0D

.. Go rules
.. _go_binary: docs/go/core/rules.md#go_binary
.. _go_context: go/toolchains.rst#go_context
.. _go_download_sdk: go/toolchains.rst#go_download_sdk
.. _go_host_sdk: go/toolchains.rst#go_host_sdk
.. _go_library: docs/go/core/rules.md#go_library
.. _go_local_sdk: go/toolchains.rst#go_local_sdk
.. _go_path: docs/go/core/rules.md#go_path
.. _go_proto_compiler: proto/core.rst#go_proto_compiler
.. _go_proto_library: proto/core.rst#go_proto_library
.. _go_register_toolchains: go/toolchains.rst#go_register_toolchains
.. _go_repository: https://github.com/bazelbuild/bazel-gazelle/blob/master/repository.md#go_repository
.. _go_rules_dependencies: go/dependencies.rst#go_rules_dependencies
.. _go_source: docs/go/core/rules.md#go_source
.. _go_test: docs/go/core/rules.md#go_test
.. _go_cross_binary: docs/go/core/rules.md#go_cross_binary
.. _go_toolchain: go/toolchains.rst#go_toolchain
.. _go_wrap_sdk: go/toolchains.rst#go_wrap_sdk
.. _gomock: docs/go/extras/extras.md#gomock

.. External rules
.. _git_repository: https://docs.bazel.build/versions/master/repo/git.html
.. _http_archive: https://docs.bazel.build/versions/master/repo/http.html#http_archive
.. _proto_library: https://github.com/bazelbuild/rules_proto

.. Issues
.. _#265: https://github.com/bazelbuild/rules_go/issues/265
.. _#721: https://github.com/bazelbuild/rules_go/issues/721
.. _#889: https://github.com/bazelbuild/rules_go/issues/889
.. _#1199: https://github.com/bazelbuild/rules_go/issues/1199
.. _#2775: https://github.com/bazelbuild/rules_go/issues/2775


Mailing list: `bazel-go-discuss`_

Slack: `#go on Bazel Slack`_, `#bazel on Go Slack`_

Announcements
-------------
2024-01-04
  Release
  `v0.44.2 <https://github.com/bazelbuild/rules_go/releases/tag/v0.44.2>`_
  is now available. This includes two bug fixes (https://github.com/bazelbuild/rules_go/pull/3808 and https://github.com/bazelbuild/rules_go/pull/3810) with
  the test timeout handler.

2023-12-29
  Release
  `v0.44.1 <https://github.com/bazelbuild/rules_go/releases/tag/v0.43.0>`_
  is now available. This release reverts a forced upgrade (https://github.com/bazelbuild/rules_go/pull/3803) of genproto from
  `v0.44.0`.

2023-12-15
  Release
  `v0.44.0 <https://github.com/bazelbuild/rules_go/releases/tag/v0.43.0>`_
  is now available. This release adds a stacktrace when a test times out and
  adds support for nogo in Bzlmod.

2023-11-20
  Release
  `v0.43.0 <https://github.com/bazelbuild/rules_go/releases/tag/v0.43.0>`_
  is now available. This release fixes a couple bugs and upgrades `x/tools`
  to the latest version.

2023-09-28
  Release
  `v0.42.0 <https://github.com/bazelbuild/rules_go/releases/tag/v0.42.0>`_
  is now available. This release fixes a couple bugs and adds support for
  patching the standard library.

2023-07-10
  Release
  `v0.41.0 <https://github.com/bazelbuild/rules_go/releases/tag/v0.41.0>`_
  is now available. rules_go no longer ship with `@go_googleapis` repo.
  It requires Gazelle v0.32.0 or later.

2023-06-28
  Release
  `v0.40.1 <https://github.com/bazelbuild/rules_go/releases/tag/v0.40.1>`_
  is now available. This release fixes a couple bugs in
  the go packages driver that were introduced in 0.40.0.

2023-06-22
  Release
  `v0.40.0 <https://github.com/bazelbuild/rules_go/releases/tag/v0.40.0>`_
  is now available. This release includes support for `//nolint` parsing,
  as well as a large set of additional changes. Please check the release notes
  for details.

2023-03-27
  Release
  `v0.39.0 <https://github.com/bazelbuild/rules_go/releases/tag/v0.39.0>`_
  is now available. This release includes a simpler interface for Bzlmod
  `go_sdk` registration, makes the `//go` tool available to users, and
  fixes various bugs.

2022-12-06
  Release
  `v0.37.0 <https://github.com/bazelbuild/rules_go/releases/tag/v0.37.0>`_
  is now available. This release includes support for generated code in
  the packages driver as well as various bugfixes.

2022-11-22
  Release
  `v0.36.0 <https://github.com/bazelbuild/rules_go/releases/tag/v0.36.0>`_
  is now available. This release adds a Go library to look up runfiles, and
  two command line flags to pass additional compiler and linker options.

2022-09-11
  Release
  `v0.35.0 <https://github.com/bazelbuild/rules_go/releases/tag/v0.35.0>`_
  is now available. This release supports switching Go SDK version from a
  build flag. Starting this release, rules_go requires Bazel 5.1.0, because
  it needs to read CPU architecture from Bazel to better support Apple M1
  with Rosetta translation.

2022-07-19
  Release
  `v0.34.0 <https://github.com/bazelbuild/rules_go/releases/tag/v0.34.0>`_
  is now available. This release brings very experimental support for bzlmod,
  several improvements to nogo and gopackagesdriver.

2022-06-06
  Release
  `v0.33.0 <https://github.com/bazelbuild/rules_go/releases/tag/v0.33.0>`_
  is now available. This release consists mostly of bug fixes and deprecates
  the `asm`, `compile`, `cover`, and `pack` actions on `go_context`.
2022-05-11
  Release
  `v0.32.0 <https://github.com/bazelbuild/rules_go/releases/tag/v0.32.0>`_
  is now available. This adds `gomock` to rules_go and supports lcov format
  for code coverage report, as well as a long list of other changes listed
  in the release notes.

Contents
--------

* `Overview`_
* `Setup`_
* `protobuf and gRPC`_
* `FAQ`_

Documentation
~~~~~~~~~~~~~

* `Core rules`_

  * `go_binary`_
  * `go_library`_
  * `go_test`_
  * `go_source`_
  * `go_path`_
  * `go_cross_binary`_

* `Proto rules`_

  * `go_proto_library`_
  * `go_proto_compiler`_

* `Dependencies`_

  * `go_rules_dependencies`_
  * `go_repository`_ (Gazelle)

* `Toolchains`_

  * `go_register_toolchains`_
  * `go_download_sdk`_
  * `go_host_sdk`_
  * `go_local_sdk`_
  * `go_wrap_sdk`_
  * `go_toolchain`_
  * `go_context`_

* `Extra rules <docs/go/extras/extras.md>`_
  * `gomock`_

* `nogo build-time static analysis`_
* `Build modes <go/modes.rst>`_

Quick links
~~~~~~~~~~~

* `rules_go and Gazelle roadmap`_
* `Deprecation schedule`_
* `Using rules_go on Windows`_

Overview
--------

The rules are in the beta stage of development. They support:

* Building libraries, binaries, and tests (`go_library`_, `go_binary`_,
  `go_test`_)
* Vendoring
* cgo
* Cross-compilation
* Generating BUILD files via gazelle_
* Build-time code analysis via nogo_
* `Protocol buffers`_
* Remote execution
* `Coverage`_
* `gopls integration`_ for editor support
* Debugging

They currently do not support or have limited support for:

* C/C++ integration other than cgo (SWIG)

The Go rules are tested and supported on the following host platforms:

* Linux, macOS, Windows
* amd64

Users have reported success on several other platforms, but the rules are
only tested on those listed above.

Note: Since version v0.38.0, rules_go requires Bazel ≥ 5.4.0 to work.

The ``master`` branch is only guaranteed to work with the latest version of Bazel.


Setup
-----

System setup
~~~~~~~~~~~~

To build Go code with Bazel, you will need:

* A recent version of Bazel.
* A C/C++ toolchain (if using cgo). Bazel will attempt to configure the
  toolchain automatically.
* Bash, ``patch``, ``cat``, and a handful of other Unix tools in ``PATH``.

You normally won't need a Go toolchain installed. Bazel will download one.

See `Using rules_go on Windows`_ for Windows-specific setup instructions.
Several additional tools need to be installed and configured.

Initial project setup
~~~~~~~~~~~~~~~~~~~~~

If you are using Bazel's new external dependency management system `Bzlmod`_,
refer to the dedicated `Go with Bzlmod`_ guide instead.

Create a file at the top of your repository named ``WORKSPACE``, and add the
snippet below (or add to your existing ``WORKSPACE``). This tells Bazel to
fetch rules_go and its dependencies. Bazel will download a recent supported
Go toolchain and register it for use.

.. code:: bzl

    load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

    http_archive(
        name = "io_bazel_rules_go",
        integrity = "sha256-fHbWI2so/2laoozzX5XeMXqUcv0fsUrHl8m/aE8Js3w=",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v0.44.2/rules_go-v0.44.2.zip",
            "https://github.com/bazelbuild/rules_go/releases/download/v0.44.2/rules_go-v0.44.2.zip",
        ],
    )

    load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")

    go_rules_dependencies()

    go_register_toolchains(version = "1.20.7")

You can use rules_go at ``master`` by using `git_repository`_ instead of
`http_archive`_ and pointing to a recent commit.

Add a file named ``BUILD.bazel`` in the root directory of your project.
You'll need a build file in each directory with Go code, but you'll also need
one in the root directory, even if your project doesn't have Go code there.
For a "Hello, world" binary, the file should look like this:

.. code:: bzl

    load("@io_bazel_rules_go//go:def.bzl", "go_binary")

    go_binary(
        name = "hello",
        srcs = ["hello.go"],
    )

You can build this target with ``bazel build //:hello``.

Generating build files
~~~~~~~~~~~~~~~~~~~~~~

If your project can be built with ``go build``, you can generate and update your
build files automatically using gazelle_.

Add the ``bazel_gazelle`` repository and its dependencies to your
``WORKSPACE``. It should look like this:

  .. code:: bzl

    load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

    http_archive(
        name = "io_bazel_rules_go",
        integrity = "sha256-fHbWI2so/2laoozzX5XeMXqUcv0fsUrHl8m/aE8Js3w=",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v0.44.2/rules_go-v0.44.2.zip",
            "https://github.com/bazelbuild/rules_go/releases/download/v0.44.2/rules_go-v0.44.2.zip",
        ],
    )

    http_archive(
        name = "bazel_gazelle",
        integrity = "sha256-MpOL2hbmcABjA1R5Bj2dJMYO2o15/Uc5Vj9Q0zHLMgk=",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-gazelle/releases/download/v0.35.0/bazel-gazelle-v0.35.0.tar.gz",
            "https://github.com/bazelbuild/bazel-gazelle/releases/download/v0.35.0/bazel-gazelle-v0.35.0.tar.gz",
        ],
    )

    load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
    load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")

    go_rules_dependencies()

    go_register_toolchains(version = "1.20.7")

    gazelle_dependencies()

Add the code below to the ``BUILD.bazel`` file in your project's root directory.
Replace the string after ``prefix`` with an import path prefix that matches your
project. It should be the same as your module path, if you have a ``go.mod``
file.

.. code:: bzl

    load("@bazel_gazelle//:def.bzl", "gazelle")

    # gazelle:prefix github.com/example/project
    gazelle(name = "gazelle")

This declares a ``gazelle`` binary rule, which you can run using the command
below:

.. code:: bash

    bazel run //:gazelle

This will generate a ``BUILD.bazel`` file with `go_library`_, `go_binary`_, and
`go_test`_ targets for each package in your project. You can run the same
command in the future to update existing build files with new source files,
dependencies, and options.

Writing build files by hand
~~~~~~~~~~~~~~~~~~~~~~~~~~~

If your project doesn't follow ``go build`` conventions or you prefer not to use
gazelle_, you can write build files by hand.

In each directory that contains Go code, create a file named ``BUILD.bazel``
Add a ``load`` statement at the top of the file for the rules you use.

.. code:: bzl

    load("@io_bazel_rules_go//go:def.bzl", "go_binary", "go_library", "go_test")

For each library, add a `go_library`_ rule like the one below.  Source files are
listed in the ``srcs`` attribute. Imported packages outside the standard library
are listed in the ``deps`` attribute using `Bazel labels`_ that refer to
corresponding `go_library`_ rules. The library's import path must be specified
with the ``importpath`` attribute.

.. code:: bzl

    go_library(
        name = "foo_library",
        srcs = [
            "a.go",
            "b.go",
        ],
        importpath = "github.com/example/project/foo",
        deps = [
            "//tools",
            "@org_golang_x_utils//stuff",
        ],
        visibility = ["//visibility:public"],
    )

For tests, add a `go_test`_ rule like the one below. The library being tested
should be listed in an ``embed`` attribute.

.. code:: bzl

    go_test(
        name = "foo_test",
        srcs = [
            "a_test.go",
            "b_test.go",
        ],
        embed = [":foo_lib"],
        deps = [
            "//testtools",
            "@org_golang_x_utils//morestuff",
        ],
    )

For binaries, add a `go_binary`_ rule like the one below.

.. code:: bzl

    go_binary(
        name = "foo",
        srcs = ["main.go"],
    )

Adding external repositories
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For each Go repository, add a `go_repository`_ rule to ``WORKSPACE`` like the
one below.  This rule comes from the Gazelle repository, so you will need to
load it. `gazelle update-repos`_ can generate or update these rules
automatically from a go.mod or Gopkg.lock file.

.. code:: bzl

    load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

    # Download the Go rules.
    http_archive(
        name = "io_bazel_rules_go",
        integrity = "sha256-fHbWI2so/2laoozzX5XeMXqUcv0fsUrHl8m/aE8Js3w=",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v0.44.2/rules_go-v0.44.2.zip",
            "https://github.com/bazelbuild/rules_go/releases/download/v0.44.2/rules_go-v0.44.2.zip",
        ],
    )

    # Download Gazelle.
    http_archive(
        name = "bazel_gazelle",
        integrity = "sha256-MpOL2hbmcABjA1R5Bj2dJMYO2o15/Uc5Vj9Q0zHLMgk=",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-gazelle/releases/download/v0.35.0/bazel-gazelle-v0.35.0.tar.gz",
            "https://github.com/bazelbuild/bazel-gazelle/releases/download/v0.35.0/bazel-gazelle-v0.35.0.tar.gz",
        ],
    )

    # Load macros and repository rules.
    load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
    load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies", "go_repository")

    # Declare Go direct dependencies.
    go_repository(
        name = "org_golang_x_net",
        importpath = "golang.org/x/net",
        sum = "h1:zK/HqS5bZxDptfPJNq8v7vJfXtkU7r9TLIoSr1bXaP4=",
        version = "v0.0.0-20200813134508-3edf25e44fcc",
    )

    # Declare indirect dependencies and register toolchains.
    go_rules_dependencies()

    go_register_toolchains(version = "1.20.7")

    gazelle_dependencies()


protobuf and gRPC
-----------------

To generate code from protocol buffers, you'll need to add a dependency on
``com_google_protobuf`` to your ``WORKSPACE``.

.. code:: bzl

    load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

    http_archive(
        name = "com_google_protobuf",
        sha256 = "d0f5f605d0d656007ce6c8b5a82df3037e1d8fe8b121ed42e536f569dec16113",
        strip_prefix = "protobuf-3.14.0",
        urls = [
            "https://mirror.bazel.build/github.com/protocolbuffers/protobuf/archive/v3.14.0.tar.gz",
            "https://github.com/protocolbuffers/protobuf/archive/v3.14.0.tar.gz",
        ],
    )

    load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

    protobuf_deps()

You'll need a C/C++ toolchain registered for the execution platform (the
platform where Bazel runs actions) to build protoc.

The `proto_library`_ rule is provided by the ``rules_proto`` repository.
``protoc-gen-go``, the Go proto compiler plugin, is provided by the
``com_github_golang_protobuf`` repository. Both are declared by
`go_rules_dependencies`_. You won't need to declare an explicit dependency
unless you specifically want to use a different version. See `Overriding
dependencies`_ for instructions on using a different version.

gRPC dependencies are not declared by default (there are too many). You can
declare them in WORKSPACE using `go_repository`_. You may want to use
`gazelle update-repos`_ to import them from ``go.mod``.

See `Proto dependencies`_, `gRPC dependencies`_ for more information. See also
`Avoiding conflicts`_.

Once all dependencies have been registered, you can declare `proto_library`_
and `go_proto_library`_ rules to generate and compile Go code from .proto
files.

.. code:: bzl

    load("@rules_proto//proto:defs.bzl", "proto_library")
    load("@io_bazel_rules_go//proto:def.bzl", "go_proto_library")

    proto_library(
        name = "foo_proto",
        srcs = ["foo.proto"],
        deps = ["//bar:bar_proto"],
        visibility = ["//visibility:public"],
    )

    go_proto_library(
        name = "foo_go_proto",
        importpath = "github.com/example/protos/foo_proto",
        protos = [":foo_proto"],
        visibility = ["//visibility:public"],
    )

A ``go_proto_library`` target may be imported and depended on like a normal
``go_library``.

Note that recent versions of rules_go support both APIv1
(``github.com/golang/protobuf``) and APIv2 (``google.golang.org/protobuf``).
By default, code is generated with
``github.com/golang/protobuf/cmd/protoc-gen-gen`` for compatibility with both
interfaces. Client code may import use either runtime library or both.

FAQ
---

**Go**

* `Can I still use the go command?`_
* `Does this work with Go modules?`_
* `What's up with the go_default_library name?`_
* `How do I cross-compile?`_
* `How do I access testdata?`_
* `How do I access go_binary executables from go_test?`_

**Protocol buffers**

* `How do I avoid conflicts with protocol buffers?`_
* `Can I use a vendored gRPC with go_proto_library?`_

**Dependencies and testing**

* `How do I use different versions of dependencies?`_
* `How do I run Bazel on Travis CI?`_
* `How do I test a beta version of the Go SDK?`_

Can I still use the go command?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Yes, but not directly.

rules_go invokes the Go compiler and linker directly, based on the targets
described with `go_binary`_ and other rules. Bazel and rules_go together
fill the same role as the ``go`` command, so it's not necessary to use the
``go`` command in a Bazel workspace.

That said, it's usually still a good idea to follow conventions required by
the ``go`` command (e.g., one package per directory, package paths match
directory paths). Tools that aren't compatible with Bazel will still work,
and your project can be depended on by non-Bazel projects.

If you need to use the ``go`` command to perform tasks that Bazel doesn't cover
(such as adding a new dependency to ``go.mod``), you can use the following Bazel
invocation to run the ``go`` binary of the Bazel-configured Go SDK:

.. code:: bash

    bazel run @io_bazel_rules_go//go -- <args>

Prefer this to running ``go`` directly since it ensures that the version of Go
is identical to the one used by rules_go.

Does this work with Go modules?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Yes, but not directly. Bazel ignores ``go.mod`` files, and all package
dependencies must be expressed through ``deps`` attributes in targets
described with `go_library`_ and other rules.

You can download a Go module at a specific version as an external repository
using `go_repository`_, a workspace rule provided by gazelle_. This will also
generate build files using gazelle_.

You can import `go_repository`_ rules from a ``go.mod`` file using
`gazelle update-repos`_.

What's up with the go_default_library name?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This was used to keep import paths consistent in libraries that can be built
with ``go build`` before the ``importpath`` attribute was available.

In order to compile and link correctly, rules_go must know the Go import path
(the string by which a package can be imported) for each library. This is now
set explicitly with the ``importpath`` attribute. Before that attribute existed,
the import path was inferred by concatenating a string from a special
``go_prefix`` rule and the library's package and label name. For example, if
``go_prefix`` was ``github.com/example/project``, for a library
``//foo/bar:bar``, rules_go would infer the import path as
``github.com/example/project/foo/bar/bar``. The stutter at the end is
incompatible with ``go build``, so if the label name was ``go_default_library``,
the import path would not include it. So for the library
``//foo/bar:go_default_library``, the import path would be
``github.com/example/project/foo/bar``.

Since ``go_prefix`` was removed and the ``importpath`` attribute became
mandatory (see `#721`_), the ``go_default_library`` name no longer serves any
purpose. We may decide to stop using it in the future (see `#265`_).

How do I cross-compile?
~~~~~~~~~~~~~~~~~~~~~~~

You can cross-compile by setting the ``--platforms`` flag on the command line.
For example:

.. code::

  $ bazel build --platforms=@io_bazel_rules_go//go/toolchain:linux_amd64 //cmd

By default, cgo is disabled when cross-compiling. To cross-compile with cgo,
add a ``_cgo`` suffix to the target platform. You must register a
cross-compiling C/C++ toolchain with Bazel for this to work.

.. code::

  $ bazel build --platforms=@io_bazel_rules_go//go/toolchain:linux_amd64_cgo //cmd

Platform-specific sources with build tags or filename suffixes are filtered
automatically at compile time. You can selectively include platform-specific
dependencies with ``select`` expressions (Gazelle does this automatically).

.. code:: bzl

  go_library(
      name = "foo",
      srcs = [
          "foo_linux.go",
          "foo_windows.go",
      ],
      deps = select({
          "@io_bazel_rules_go//go/platform:linux_amd64": [
              "//bar_linux",
          ],
          "@io_bazel_rules_go//go/platform:windows_amd64": [
              "//bar_windows",
          ],
          "//conditions:default": [],
      }),
  )

To build a specific `go_binary`_ target for a target platform or using a
specific golang SDK version, use the `go_cross_binary`_ rule. This is useful
for producing multiple binaries for different platforms in a single build.

To build a specific `go_test`_ target for a target platform, set the
``goos`` and ``goarch`` attributes on that rule.

You can equivalently depend on a `go_binary`_ or `go_test`_ rule through
a Bazel `configuration transition`_ on ``//command_line_option:platforms``
(there are problems with this approach prior to rules_go 0.23.0).

How do I access testdata?
~~~~~~~~~~~~~~~~~~~~~~~~~

Bazel executes tests in a sandbox, which means tests don't automatically have
access to files. You must include test files using the ``data`` attribute.
For example, if you want to include everything in the ``testdata`` directory:

.. code:: bzl

  go_test(
      name = "foo_test",
      srcs = ["foo_test.go"],
      data = glob(["testdata/**"]),
      importpath = "github.com/example/project/foo",
  )

By default, tests are run in the directory of the build file that defined them.
Note that this follows the Go testing convention, not the Bazel convention
followed by other languages, which run in the repository root. This means
that you can access test files using relative paths. You can change the test
directory using the ``rundir`` attribute. See go_test_.

Gazelle will automatically add a ``data`` attribute like the one above if you
have a ``testdata`` directory *unless* it contains buildable .go files or
build files, in which case, ``testdata`` is treated as a normal package.

Note that on Windows, data files are not directly available to tests, since test
data files rely on symbolic links, and by default, Windows doesn't let
unprivileged users create symbolic links. You can use the
`github.com/bazelbuild/rules_go/go/tools/bazel`_ library to access data files.

How do I access go_binary executables from go_test?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The location where ``go_binary`` writes its executable file is not stable across
rules_go versions and should not be depended upon. The parent directory includes
some configuration data in its name. This prevents Bazel's cache from being
poisoned when the same binary is built in different configurations. The binary
basename may also be platform-dependent: on Windows, we add an .exe extension.

To depend on an executable in a ``go_test`` rule, reference the executable
in the ``data`` attribute (to make it visible), then expand the location
in ``args``. The real location will be passed to the test on the command line.
For example:

.. code:: bzl

  go_binary(
      name = "cmd",
      srcs = ["cmd.go"],
  )

  go_test(
      name = "cmd_test",
      srcs = ["cmd_test.go"],
      args = ["$(location :cmd)"],
      data = [":cmd"],
  )

See `//tests/core/cross`_ for a full example of a test that
accesses a binary.

Alternatively, you can set the ``out`` attribute of `go_binary`_ to a specific
filename. Note that when ``out`` is set, the binary won't be cached when
changing configurations.

.. code:: bzl

  go_binary(
      name = "cmd",
      srcs = ["cmd.go"],
      out = "cmd",
  )

  go_test(
      name = "cmd_test",
      srcs = ["cmd_test.go"],
      data = [":cmd"],
  )

How do I avoid conflicts with protocol buffers?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

See `Avoiding conflicts`_ in the proto documentation.

Can I use a vendored gRPC with go_proto_library?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This is not supported. When using `go_proto_library`_ with the
``@io_bazel_rules_go//proto:go_grpc`` compiler, an implicit dependency is added
on ``@org_golang_google_grpc//:go_default_library``. If you link another copy of
the same package from ``//vendor/google.golang.org/grpc:go_default_library``
or anywhere else, you may experience conflicts at compile or run-time.

If you're using Gazelle with proto rule generation enabled, imports of
``google.golang.org/grpc`` will be automatically resolved to
``@org_golang_google_grpc//:go_default_library`` to avoid conflicts. The
vendored gRPC should be ignored in this case.

If you specifically need to use a vendored gRPC package, it's best to avoid
using ``go_proto_library`` altogether. You can check in pre-generated .pb.go
files and build them with ``go_library`` rules. Gazelle will generate these
rules when proto rule generation is disabled (add ``# gazelle:proto
disable_global`` to your root build file).

How do I use different versions of dependencies?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

See `Overriding dependencies`_ for instructions on overriding repositories
declared in `go_rules_dependencies`_.


How do I run Bazel on Travis CI?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

References:

* `Running Bazel Tests on Travis CI`_ by Kevin Burke
* `korfuri/bazel-travis Use Bazel with Travis CI`_

In order to run Bazel tests on Travis CI, you'll need to install Bazel in the
``before_install`` script. See our configuration file linked above.

You'll want to run Bazel with a number of flags to prevent it from consuming
a huge amount of memory in the test environment.

* ``--host_jvm_args=-Xmx500m --host_jvm_args=-Xms500m``: Set the maximum and
  initial JVM heap size. Keeping the same means the JVM won't spend time
  growing the heap. The choice of heap size is somewhat arbitrary; other
  configuration files recommend limits as high as 2500m. Higher values mean
  a faster build, but higher risk of OOM kill.
* ``--bazelrc=.test-bazelrc``: Use a Bazel configuration file specific to
  Travis CI. You can put most of the remaining options in here.
* ``build --spawn_strategy=standalone --genrule_strategy=standalone``: Disable
  sandboxing for the build. Sandboxing may fail inside of Travis's containers
  because the ``mount`` system call is not permitted.
* ``test --test_strategy=standalone``: Disable sandboxing for tests as well.
* ``--local_resources=1536,1.5,0.5``: Set Bazel limits on available RAM in MB,
  available cores for compute, and available cores for I/O. Higher values
  mean a faster build, but higher contention and risk of OOM kill.
* ``--noshow_progress``: Suppress progress messages in output for cleaner logs.
* ``--verbose_failures``: Get more detailed failure messages.
* ``--test_output=errors``: Show test stderr in the Travis log. Normally,
  test output is written log files which Travis does not save or report.

Downloads on Travis are relatively slow (the network is heavily
contended), so you'll want to minimize the amount of network I/O in
your build. Downloading Bazel and a Go SDK is a huge part of that. To
avoid downloading a Go SDK, you may request a container with a
preinstalled version of Go in your ``.travis.yml`` file, then call
``go_register_toolchains(go_version = "host")`` in a Travis-specific
``WORKSPACE`` file.

You may be tempted to put Bazel's cache in your Travis cache. Although this
can speed up your build significantly, Travis stores its cache on Amazon, and
it takes a very long time to transfer. Clean builds seem faster in practice.

How do I test a beta version of the Go SDK?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

rules_go only supports official releases of the Go SDK. However, you can still
test beta and RC versions by passing a ``version`` like ``"1.16beta1"`` to
`go_register_toolchains`_. See also `go_download_sdk`_.

.. code:: bzl

  load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")

  go_rules_dependencies()

  go_register_toolchains(version = "1.17beta1")
