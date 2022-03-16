# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
load("//build_config:build_config.bzl", "build_config")
load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "base",
    hdrs = [
        "compiler_specific.h",
        "containers/checked_iterators.h",
        "containers/contains.h",
        "containers/contiguous_iterator.h",
        "containers/span.h",
        "containers/util.h",
        "cxx17_backports.h",
        "cxx20_to_address.h",
        "debug/leak_annotations.h",
        "functional/identity.h",
        "functional/invoke.h",
        "functional/not_fn.h",
        "macros.h",
        "no_destructor.h",
        "ranges/algorithm.h",
        "ranges/functional.h",
        "ranges/ranges.h",
        "stl_util.h",
        "template_util.h",
    ],
    copts = build_config.default_copts,
    visibility = ["//visibility:public"],
    deps = [
        "//build:build_config",
        "//polyfills",
        "@com_google_absl//absl/types:optional",
    ],
)
