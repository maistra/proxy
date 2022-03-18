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

def go_rules_dependencies(is_rules_go = False):
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
    _maybe(
        http_archive,
        name = "platforms",
        # 0.0.4, latest as of 2021-03-05
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/platforms/releases/download/0.0.4/platforms-0.0.4.tar.gz",
            "https://github.com/bazelbuild/platforms/releases/download/0.0.4/platforms-0.0.4.tar.gz",
        ],
        sha256 = "079945598e4b6cc075846f7fd6a9d0857c33a7afc0de868c2ccb96405225135d",
    )

    # Needed by rules_go implementation and tests.
    # We can't call bazel_skylib_workspace from here. At the moment, it's only
    # used to register unittest toolchains, which rules_go does not need.
    _maybe(
        http_archive,
        name = "bazel_skylib",
        # 1.0.3, latest as of 2021-03-05
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.0.3/bazel-skylib-1.0.3.tar.gz",
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.0.3/bazel-skylib-1.0.3.tar.gz",
        ],
        sha256 = "1c531376ac7e5a180e0237938a2536de0c54d93f5c278634818e0efc952dd56c",
    )

    # Needed for nogo vet checks and go/packages.
    _maybe(
        http_archive,
        name = "org_golang_x_tools",
        # v0.1.0, as of 2021-03-17
        urls = [
            "https://mirror.bazel.build/github.com/golang/tools/archive/v0.1.0.zip",
            "https://github.com/golang/tools/archive/v0.1.0.zip",
        ],
        sha256 = "60a5cee8304b4d9130344f156a10ba648e315b5fca4b84939b765b26ce217dee",
        strip_prefix = "tools-0.1.0",
        patches = [
            # deletegopls removes the gopls subdirectory. It contains a nested
            # module with additional dependencies. It's not needed by rules_go.
            "@io_bazel_rules_go//third_party:org_golang_x_tools-deletegopls.patch",
            # gazelle args: -repo_root . -go_prefix golang.org/x/tools -go_naming_convention import_alias
            "@io_bazel_rules_go//third_party:org_golang_x_tools-gazelle.patch",
        ],
        patch_args = ["-p1"],
    )

    _maybe(
        http_archive,
        name = "org_golang_x_sys",
        # master, as of 2021-03-17
        urls = [
            "https://github.com/golang/sys/archive/390168757d9c647283340d526204e3409d5903f3.zip",
            "https://mirror.bazel.build/github.com/golang/sys/archive/390168757d9c647283340d526204e3409d5903f3.zip",
        ],
        sha256 = "1e7128237f37a9e28f3ea08267ea95f0cd32cbe20c5a25c99430697001de85b5",
        strip_prefix = "sys-390168757d9c647283340d526204e3409d5903f3",
        patches = [
            # gazelle args: -repo_root . -go_prefix golang.org/x/sys -go_naming_convention import_alias
            "@io_bazel_rules_go//third_party:org_golang_x_sys-gazelle.patch",
        ],
        patch_args = ["-p1"],
    )

    # Needed by golang.org/x/tools/go/packages
    _maybe(
        http_archive,
        name = "org_golang_x_xerrors",
        # master, as of 2021-03-05
        urls = [
            "https://mirror.bazel.build/github.com/golang/xerrors/archive/5ec99f83aff198f5fbd629d6c8d8eb38a04218ca.zip",
            "https://github.com/golang/xerrors/archive/5ec99f83aff198f5fbd629d6c8d8eb38a04218ca.zip",
        ],
        sha256 = "cd9de801daf63283be91a76d7f91e8a9541798c5c0e8bcfb7ee804b78a493b02",
        strip_prefix = "xerrors-5ec99f83aff198f5fbd629d6c8d8eb38a04218ca",
        patches = [
            # gazelle args: -repo_root -go_prefix golang.org/x/xerrors -go_naming_convention import_alias
            "@io_bazel_rules_go//third_party:org_golang_x_xerrors-gazelle.patch",
        ],
        patch_args = ["-p1"],
    )

    # Needed for additional targets declared around binaries with c-archive
    # and c-shared link modes.
    _maybe(
        http_archive,
        name = "rules_cc",
        # master, as of 2021-03-05
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_cc/archive/88ef31b429631b787ceb5e4556d773b20ad797c8.zip",
            "https://github.com/bazelbuild/rules_cc/archive/88ef31b429631b787ceb5e4556d773b20ad797c8.zip",
        ],
        sha256 = "92a89a2bbe6c6db2a8b87da4ce723aff6253656e8417f37e50d362817c39b98b",
        strip_prefix = "rules_cc-88ef31b429631b787ceb5e4556d773b20ad797c8",
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
    _maybe(
        http_archive,
        name = "org_golang_google_protobuf",
        sha256 = "62992b0f5864aee2077a6cffa57a2d2bd30e7af4b6745eebd816dcde3526002f",
        # v1.25.0, latest as of 2021-03-05
        urls = [
            "https://mirror.bazel.build/github.com/protocolbuffers/protobuf-go/archive/v1.25.0.zip",
            "https://github.com/protocolbuffers/protobuf-go/archive/v1.25.0.zip",
        ],
        strip_prefix = "protobuf-go-1.25.0",
        patches = [
            # gazelle args: -repo_root . -go_prefix google.golang.org/protobuf -go_naming_convention import_alias -proto disable_global
            "@io_bazel_rules_go//third_party:org_golang_google_protobuf-gazelle.patch",
        ],
        patch_args = ["-p1"],
    )

    # Legacy protobuf compiler, runtime, and utilities.
    # We still use protoc-gen-go because the new one doesn't support gRPC, and
    # the gRPC compiler doesn't exist yet.
    # We need to apply a patch to enable both go_proto_library and
    # go_library with pre-generated sources.
    _maybe(
        http_archive,
        name = "com_github_golang_protobuf",
        # v1.4.3, latest as of 2021-03-05
        urls = [
            "https://mirror.bazel.build/github.com/golang/protobuf/archive/v1.4.3.zip",
            "https://github.com/golang/protobuf/archive/v1.4.3.zip",
        ],
        sha256 = "836d26511eb4282411a2fcb1e61b538163811612f93f8ca22d034628d41819d9",
        strip_prefix = "protobuf-1.4.3",
        patches = [
            # gazelle args: -repo_root . -go_prefix github.com/golang/protobuf -go_naming_convention import_alias -proto disable_global
            "@io_bazel_rules_go//third_party:com_github_golang_protobuf-gazelle.patch",
        ],
        patch_args = ["-p1"],
    )

    # Extra protoc plugins and libraries.
    # Doesn't belong here, but low maintenance.
    _maybe(
        http_archive,
        name = "com_github_mwitkow_go_proto_validators",
        # v0.3.2, latest as of 2021-03-05
        urls = [
            "https://mirror.bazel.build/github.com/mwitkow/go-proto-validators/archive/v0.3.2.zip",
            "https://github.com/mwitkow/go-proto-validators/archive/v0.3.2.zip",
        ],
        sha256 = "d8697f05a2f0eaeb65261b480e1e6035301892d9fc07ed945622f41b12a68142",
        strip_prefix = "go-proto-validators-0.3.2",
        # Bazel support added in v0.3.0, so no patches needed.
    )

    _maybe(
        http_archive,
        name = "com_github_gogo_protobuf",
        # v1.3.2, latest as of 2021-03-05
        urls = [
            "https://mirror.bazel.build/github.com/gogo/protobuf/archive/v1.3.2.zip",
            "https://github.com/gogo/protobuf/archive/v1.3.2.zip",
        ],
        sha256 = "f89f8241af909ce3226562d135c25b28e656ae173337b3e58ede917aa26e1e3c",
        strip_prefix = "protobuf-1.3.2",
        patches = [
            # gazelle args: -repo_root . -go_prefix github.com/gogo/protobuf -go_naming_convention import_alias -proto legacy
            "@io_bazel_rules_go//third_party:com_github_gogo_protobuf-gazelle.patch",
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
    _maybe(
        http_archive,
        name = "org_golang_google_genproto",
        # master, as of 2021-03-05
        urls = [
            "https://mirror.bazel.build/github.com/googleapis/go-genproto/archive/9728d6b83eeb3850506175f3b213d2037ce1f89d.zip",
            "https://github.com/googleapis/go-genproto/archive/9728d6b83eeb3850506175f3b213d2037ce1f89d.zip",
        ],
        sha256 = "faff880420132f1f1e32d7865d0361bd4876683afa031dae113fd4bb94ba0d2d",
        strip_prefix = "go-genproto-9728d6b83eeb3850506175f3b213d2037ce1f89d",
        patches = [
            # gazelle args: -repo_root . -go_prefix google.golang.org/genproto -go_naming_convention import_alias -proto disable_global
            "@io_bazel_rules_go//third_party:org_golang_google_genproto-gazelle.patch",
        ],
        patch_args = ["-p1"],
    )

    # go_proto_library targets for gRPC and Google APIs.
    # TODO(#1986): migrate to com_google_googleapis. This workspace was added
    # before the real workspace supported Bazel. Gazelle resolves dependencies
    # here. Gazelle should resolve dependencies to com_google_googleapis
    # instead, and we should remove this.
    _maybe(
        http_archive,
        name = "go_googleapis",
        # master, as of 2021-03-05
        urls = [
            "https://mirror.bazel.build/github.com/googleapis/googleapis/archive/d4cd8d96ed6eb5dd7c997aab68a1d6bb0825090c.zip",
            "https://github.com/googleapis/googleapis/archive/d4cd8d96ed6eb5dd7c997aab68a1d6bb0825090c.zip",
        ],
        sha256 = "711bc79bd40406dda685a8633f7478979baabaab19eeac664d53f7621866bebc",
        strip_prefix = "googleapis-d4cd8d96ed6eb5dd7c997aab68a1d6bb0825090c",
        patches = [
            # find . -name BUILD.bazel -delete
            "@io_bazel_rules_go//third_party:go_googleapis-deletebuild.patch",
            # set gazelle directives; change workspace name
            "@io_bazel_rules_go//third_party:go_googleapis-directives.patch",
            # gazelle args: -repo_root .
            "@io_bazel_rules_go//third_party:go_googleapis-gazelle.patch",
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

    go_name_hack(
        name = "io_bazel_rules_go_name_hack",
        is_rules_go = is_rules_go,
    )

def _maybe(repo_rule, name, **kwargs):
    if name not in native.existing_rules():
        repo_rule(name = name, **kwargs)

def _go_name_hack_impl(ctx):
    build_content = """\
load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

bzl_library(
    name = "def",
    srcs = ["def.bzl"],
)
"""
    ctx.file("BUILD.bazel", build_content)
    content = "IS_RULES_GO = {}".format(ctx.attr.is_rules_go)
    ctx.file("def.bzl", content)

go_name_hack = repository_rule(
    implementation = _go_name_hack_impl,
    attrs = {
        "is_rules_go": attr.bool(),
    },
    doc = """go_name_hack records whether the main workspace is rules_go.

See documentation for _filter_transition_label in
go/private/rules/transition.bzl.
""",
)
