# Copyright 2017 Istio Authors. All Rights Reserved.
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
#
################################################################################
#
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

GOOGLETEST = "d225acc90bc3a8c420a9bcd1f033033c1ccd7fe0"
GOOGLETEST_SHA256 = "01508c8f47c99509130f128924f07f3a60be05d039cff571bb11d60bb11a3581"

def googletest_repositories(bind = True):
    BUILD = """
# Copyright 2017 Istio Authors. All Rights Reserved.
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
#
################################################################################
#
cc_library(
    name = "googletest",
    srcs = [
        "googletest/src/gtest-all.cc",
        "googlemock/src/gmock-all.cc",
    ],
    hdrs = glob([
        "googletest/include/**/*.h",
        "googlemock/include/**/*.h",
        "googletest/src/*.cc",
        "googletest/src/*.h",
        "googlemock/src/*.cc",
    ]),
    includes = [
        "googlemock",
        "googletest",
        "googletest/include",
        "googlemock/include",
    ],
    visibility = ["//visibility:public"],
)
cc_library(
    name = "googletest_main",
    srcs = ["googlemock/src/gmock_main.cc"],
    visibility = ["//visibility:public"],
    deps = [":googletest"],
)
cc_library(
    name = "googletest_prod",
    hdrs = [
        "googletest/include/gtest/gtest_prod.h",
    ],
    includes = [
        "googletest/include",
    ],
    visibility = ["//visibility:public"],
)
"""
    http_archive(
        name = "googletest_git",
        build_file_content = BUILD,
        strip_prefix = "googletest-" + GOOGLETEST,
        url = "https://github.com/google/googletest/archive/" + GOOGLETEST + ".tar.gz",
        sha256 = GOOGLETEST_SHA256,
    )

    if bind:
        native.bind(
            name = "googletest",
            actual = "@googletest_git//:googletest",
        )

        native.bind(
            name = "googletest_main",
            actual = "@googletest_git//:googletest_main",
        )

        native.bind(
            name = "googletest_prod",
            actual = "@googletest_git//:googletest_prod",
        )

#
# To update these...
# 1) find the ISTIO_API SHA you want in git
# 2) wget https://github.com/istio/api/archive/$ISTIO_API_SHA.tar.gz && sha256sum $ISTIO_API_SHA.tar.gz
#
ISTIO_API = "75bb24b620144218d26b92afedbb428e4d84e506"
ISTIO_API_SHA256 = "c72602c38f7ab10e430618e4ce82fee143f4446d468863ba153ea897bdff2298"

def istioapi_repositories(bind = True):
    BUILD = """
# Copyright 2018 Istio Authors. All Rights Reserved.
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
#
################################################################################
#

proto_library(
    name = "authentication_policy_config_proto_lib",
    srcs = glob(
        ["envoy/config/filter/http/authn/v2alpha1/*.proto",
         "authentication/v1alpha1/*.proto",
         "common/v1alpha1/*.proto",
        ],
    ),
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_googleapis//google/api:field_behavior_proto",
    ],
)

cc_proto_library(
    name = "authentication_policy_config_cc_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":authentication_policy_config_proto_lib",
    ],
)

proto_library(
    name = "alpn_filter_config_proto_lib",
    srcs = glob(
        ["envoy/config/filter/http/alpn/v2alpha1/*.proto", ],
    ),
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "alpn_filter_config_cc_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":alpn_filter_config_proto_lib",
    ],
)

proto_library(
    name = "tcp_cluster_rewrite_config_proto_lib",
    srcs = glob(
        ["envoy/config/filter/network/tcp_cluster_rewrite/v2alpha1/*.proto", ],
    ),
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "tcp_cluster_rewrite_config_cc_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":tcp_cluster_rewrite_config_proto_lib",
    ],
)

"""
    http_archive(
        name = "istioapi_git",
        build_file_content = BUILD,
        strip_prefix = "api-" + ISTIO_API,
        url = "https://github.com/istio/api/archive/" + ISTIO_API + ".tar.gz",
        sha256 = ISTIO_API_SHA256,
    )
    if bind:
        native.bind(
            name = "authentication_policy_config_cc_proto",
            actual = "@istioapi_git//:authentication_policy_config_cc_proto",
        )
        native.bind(
            name = "alpn_filter_config_cc_proto",
            actual = "@istioapi_git//:alpn_filter_config_cc_proto",
        )
        native.bind(
            name = "tcp_cluster_rewrite_config_cc_proto",
            actual = "@istioapi_git//:tcp_cluster_rewrite_config_cc_proto",
        )

def istioapi_dependencies():
    istioapi_repositories()

def docker_dependencies():
    http_archive(
        name = "io_bazel_rules_docker",
        sha256 = "b1e80761a8a8243d03ebca8845e9cc1ba6c82ce7c5179ce2b295cd36f7e394bf",
        urls = ["https://github.com/bazelbuild/rules_docker/releases/download/v0.25.0/rules_docker-v0.25.0.tar.gz"],
    )
