# Copyright 2014 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Once nested repositories work, this file should cease to exist.

load("//go/private:common.bzl", "MINIMUM_BAZEL_VERSION")
load("//go/private/skylib/lib:versions.bzl", "versions")
load("//go/private:nogo.bzl", "DEFAULT_NOGO", "go_register_nogo")
load("//proto:gogo.bzl", "gogo_special_proto")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def go_rules_dependencies():
    """Declares workspaces the Go rules depend on. Workspaces that use
    rules_go should call this.

    See https://github.com/bazelbuild/rules_go/blob/master/go/dependencies.rst#overriding-dependencies
    for information on each dependency.

    Instructions for updating this file are in
    https://github.com/bazelbuild/rules_go/wiki/Updating-dependencies.

    PRs updating dependencies are NOT ACCEPTED. See
    https://github.com/bazelbuild/rules_go/blob/master/go/dependencies.rst#overriding-dependencies
    for information on choosing different versions of these repositories
    in your own project.
    """
    if getattr(native, "bazel_version", None):
        versions.check(MINIMUM_BAZEL_VERSION, bazel_version = native.bazel_version)

    # Repository of standard constraint settings and values.
    # Bazel declares this automatically after 0.28.0, but it's better to
    # define an explicit version.
    # releaser:upgrade-dep bazelbuild platforms
    _maybe(
        http_archive,
        name = "platforms",
        # 0.0.5, latest as of 2022-03-14
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/platforms/releases/download/0.0.5/platforms-0.0.5.tar.gz",
            "https://github.com/bazelbuild/platforms/releases/download/0.0.5/platforms-0.0.5.tar.gz",
        ],
        sha256 = "379113459b0feaf6bfbb584a91874c065078aa673222846ac765f86661c27407",
        strip_prefix = "",
    )

    # Needed by rules_go implementation and tests.
    # We can't call bazel_skylib_workspace from here. At the moment, it's only
    # used to register unittest toolchains, which rules_go does not need.
    # releaser:upgrade-dep bazelbuild bazel-skylib
    _maybe(
        http_archive,
        name = "bazel_skylib",
        # 1.2.1, latest as of 2022-03-14
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.2.1/bazel-skylib-1.2.1.tar.gz",
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.2.1/bazel-skylib-1.2.1.tar.gz",
        ],
        sha256 = "f7be3474d42aae265405a592bb7da8e171919d74c16f082a5457840f06054728",
        strip_prefix = "",
    )

    # Needed for nogo vet checks and go/packages.
    # releaser:upgrade-dep golang tools
    _maybe(
        http_archive,
        name = "org_golang_x_tools",
        # v0.1.9, latest as of 2022-03-14
        urls = [
            "https://mirror.bazel.build/github.com/golang/tools/archive/v0.1.9.zip",
            "https://github.com/golang/tools/archive/v0.1.9.zip",
        ],
        sha256 = "1d338afb3cd8013cfb035da6831dea2210efb0386c17b9c99b5e84724e3d733a",
        strip_prefix = "tools-0.1.9",
        patches = [
            # deletegopls removes the gopls subdirectory. It contains a nested
            # module with additional dependencies. It's not needed by rules_go.
            # releaser:patch-cmd rm -rf gopls
            Label("//third_party:org_golang_x_tools-deletegopls.patch"),
            # releaser:patch-cmd gazelle -repo_root . -go_prefix golang.org/x/tools -go_naming_convention import_alias
            Label("//third_party:org_golang_x_tools-gazelle.patch"),
        ],
        patch_args = ["-p1"],
    )

    # releaser:upgrade-dep golang sys
    _maybe(
        http_archive,
        name = "org_golang_x_sys",
        # master, as of 2022-03-14
        urls = [
            "https://mirror.bazel.build/github.com/golang/sys/archive/b874c991c1a50803422b257fb721b0b2dee3cf72.zip",
            "https://github.com/golang/sys/archive/b874c991c1a50803422b257fb721b0b2dee3cf72.zip",
        ],
        sha256 = "1b1e185f2ad0c6d66b4847c274c95c1271071746fe7d07d258151f4e9caf5d08",
        strip_prefix = "sys-b874c991c1a50803422b257fb721b0b2dee3cf72",
        patches = [
            # releaser:patch-cmd gazelle -repo_root . -go_prefix golang.org/x/sys -go_naming_convention import_alias
            Label("//third_party:org_golang_x_sys-gazelle.patch"),
        ],
        patch_args = ["-p1"],
    )

    # Needed by golang.org/x/tools/go/packages
    # releaser:upgrade-dep golang xerrors
    _maybe(
        http_archive,
        name = "org_golang_x_xerrors",
        # master, as of 2022-03-14
        urls = [
            "https://mirror.bazel.build/github.com/golang/xerrors/archive/5ec99f83aff198f5fbd629d6c8d8eb38a04218ca.zip",
            "https://github.com/golang/xerrors/archive/5ec99f83aff198f5fbd629d6c8d8eb38a04218ca.zip",
        ],
        sha256 = "cd9de801daf63283be91a76d7f91e8a9541798c5c0e8bcfb7ee804b78a493b02",
        strip_prefix = "xerrors-5ec99f83aff198f5fbd629d6c8d8eb38a04218ca",
        patches = [
            # releaser:patch-cmd gazelle -repo_root . -go_prefix golang.org/x/xerrors -go_naming_convention import_alias
            Label("//third_party:org_golang_x_xerrors-gazelle.patch"),
        ],
        patch_args = ["-p1"],
    )

    # Proto dependencies
    # These are limited as much as possible. In most cases, users need to
    # declare these on their own (probably via go_repository rules generated
    # with 'gazelle update-repos -from_file=go.mod). There are several
    # reasons for this:
    #
    # * com_google_protobuf has its own dependency macro. We can't load
    #   the macro here.
    # * rules_proto also has a dependency macro. It's only needed by tests and
    #   by gogo_special_proto. Users will need to declare it anyway.
    # * org_golang_google_grpc has too many dependencies for us to maintain.
    # * In general, declaring dependencies here confuses users when they
    #   declare their own dependencies later. Bazel ignores these.
    # * Most proto repos are updated more frequently than rules_go, and
    #   we can't keep up.

    # Go protobuf runtime library and utilities.
    # releaser:upgrade-dep protocolbuffers protobuf-go
    _maybe(
        http_archive,
        name = "org_golang_google_protobuf",
        sha256 = "a01b25899f598fbac3c2626376b74fc15229f009904c262658f8da1c1c392386",
        # v1.27.1, latest as of 2022-03-14
        urls = [
            "https://mirror.bazel.build/github.com/protocolbuffers/protobuf-go/archive/v1.27.1.zip",
            "https://github.com/protocolbuffers/protobuf-go/archive/v1.27.1.zip",
        ],
        strip_prefix = "protobuf-go-1.27.1",
        patches = [
            # releaser:patch-cmd gazelle -repo_root . -go_prefix google.golang.org/protobuf -go_naming_convention import_alias -proto disable_global
            Label("//third_party:org_golang_google_protobuf-gazelle.patch"),
        ],
        patch_args = ["-p1"],
    )

    # Legacy protobuf compiler, runtime, and utilities.
    # We still use protoc-gen-go because the new one doesn't support gRPC, and
    # the gRPC compiler doesn't exist yet.
    # We need to apply a patch to enable both go_proto_library and
    # go_library with pre-generated sources.
    # releaser:upgrade-dep golang protobuf
    _maybe(
        http_archive,
        name = "com_github_golang_protobuf",
        # v1.5.2, latest as of 2022-03-14
        urls = [
            "https://mirror.bazel.build/github.com/golang/protobuf/archive/v1.5.2.zip",
            "https://github.com/golang/protobuf/archive/v1.5.2.zip",
        ],
        sha256 = "5bd0a70e2f3829db9d0e340887af4e921c5e0e5bb3f8d1be49a934204cb16445",
        strip_prefix = "protobuf-1.5.2",
        patches = [
            # releaser:patch-cmd gazelle -repo_root . -go_prefix github.com/golang/protobuf -go_naming_convention import_alias -proto disable_global
            Label("//third_party:com_github_golang_protobuf-gazelle.patch"),
        ],
        patch_args = ["-p1"],
    )

    # Extra protoc plugins and libraries.
    # Doesn't belong here, but low maintenance.
    # releaser:upgrade-dep mwitkow go-proto-validators
    _maybe(
        http_archive,
        name = "com_github_mwitkow_go_proto_validators",
        # v0.3.2, latest as of 2022-03-14
        urls = [
            "https://mirror.bazel.build/github.com/mwitkow/go-proto-validators/archive/v0.3.2.zip",
            "https://github.com/mwitkow/go-proto-validators/archive/v0.3.2.zip",
        ],
        sha256 = "d8697f05a2f0eaeb65261b480e1e6035301892d9fc07ed945622f41b12a68142",
        strip_prefix = "go-proto-validators-0.3.2",
        # Bazel support added in v0.3.0, so no patches needed.
    )

    # releaser:upgrade-dep gogo protobuf
    _maybe(
        http_archive,
        name = "com_github_gogo_protobuf",
        # v1.3.2, latest as of 2022-03-14
        urls = [
            "https://mirror.bazel.build/github.com/gogo/protobuf/archive/v1.3.2.zip",
            "https://github.com/gogo/protobuf/archive/v1.3.2.zip",
        ],
        sha256 = "f89f8241af909ce3226562d135c25b28e656ae173337b3e58ede917aa26e1e3c",
        strip_prefix = "protobuf-1.3.2",
        patches = [
            # releaser:patch-cmd gazelle -repo_root . -go_prefix github.com/gogo/protobuf -go_naming_convention import_alias -proto legacy
            Label("//third_party:com_github_gogo_protobuf-gazelle.patch"),
        ],
        patch_args = ["-p1"],
    )

    _maybe(
        gogo_special_proto,
        name = "gogo_special_proto",
    )

    # go_library targets with pre-generated sources for Well Known Types
    # and Google APIs.
    # Doesn't belong here, but it would be an annoying source of errors if
    # this weren't generated with -proto disable_global.
    # releaser:upgrade-dep googleapis go-genproto
    _maybe(
        http_archive,
        name = "org_golang_google_genproto",
        # main, as of 2022-03-14
        urls = [
            "https://mirror.bazel.build/github.com/googleapis/go-genproto/archive/57ef72a4c106086f9e50d41d88cdb1dbbce5486e.zip",
            "https://github.com/googleapis/go-genproto/archive/57ef72a4c106086f9e50d41d88cdb1dbbce5486e.zip",
        ],
        sha256 = "6fec9c3360c896545f10b5271b90ec30829fd079c105f0aeb33ebdbb1d74ecfa",
        strip_prefix = "go-genproto-57ef72a4c106086f9e50d41d88cdb1dbbce5486e",
        patches = [
            # releaser:patch-cmd gazelle -repo_root . -go_prefix google.golang.org/genproto -go_naming_convention import_alias -proto disable_global
            Label("//third_party:org_golang_google_genproto-gazelle.patch"),
        ],
        patch_args = ["-p1"],
    )

    # go_proto_library targets for gRPC and Google APIs.
    # TODO(#1986): migrate to com_google_googleapis. This workspace was added
    # before the real workspace supported Bazel. Gazelle resolves dependencies
    # here. Gazelle should resolve dependencies to com_google_googleapis
    # instead, and we should remove this.
    # releaser:upgrade-dep googleapis googleapis
    _maybe(
        http_archive,
        name = "go_googleapis",
        # master, as of 2022-03-14
        urls = [
            "https://mirror.bazel.build/github.com/googleapis/googleapis/archive/9acf39829240ef41f5adb762a29b87bc6eeee728.zip",
            "https://github.com/googleapis/googleapis/archive/9acf39829240ef41f5adb762a29b87bc6eeee728.zip",
        ],
        sha256 = "69712ea6f6a132e77d788b9b0ed8e974e0a3d377e88c7283f6950e13c4e60438",
        strip_prefix = "googleapis-9acf39829240ef41f5adb762a29b87bc6eeee728",
        patches = [
            # releaser:patch-cmd find . -name BUILD.bazel -delete
            Label("//third_party:go_googleapis-deletebuild.patch"),
            # set gazelle directives; change workspace name
            Label("//third_party:go_googleapis-directives.patch"),
            # releaser:patch-cmd gazelle -repo_root .
            Label("//third_party:go_googleapis-gazelle.patch"),
        ],
        patch_args = ["-E", "-p1"],
    )

    # This may be overridden by go_register_toolchains, but it's not mandatory
    # for users to call that function (they may declare their own @go_sdk and
    # register their own toolchains).
    _maybe(
        go_register_nogo,
        name = "io_bazel_rules_nogo",
        nogo = DEFAULT_NOGO,
    )

def _maybe(repo_rule, name, **kwargs):
    if name not in native.existing_rules():
        repo_rule(name = name, **kwargs)
