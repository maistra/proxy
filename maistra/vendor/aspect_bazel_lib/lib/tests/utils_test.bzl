"Unit tests for test.bzl"

load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")
load("//lib/private:utils.bzl", "utils")

def _to_label_test_impl(ctx):
    env = unittest.begin(ctx)

    # assert label/string comparisons work as expected
    asserts.true(env, "//some/label" != Label("//some/label"))
    asserts.true(env, "//some/label" != Label("//some/other/label"))
    asserts.true(env, Label("//some/label") == Label("//some/label"))
    asserts.true(env, "//some/label" != Label("//some/label"))
    asserts.true(env, "//some/label" != Label("//some/other/label"))

    # assert that to_label can convert from string to label
    asserts.true(env, utils.to_label("//hello/world") == Label("//hello/world:world"))
    asserts.true(env, utils.to_label("//hello/world:world") == Label("//hello/world:world"))

    # assert that to_label will handle a Label as an argument
    asserts.true(env, utils.to_label(Label("//hello/world")) == Label("//hello/world:world"))
    asserts.true(env, utils.to_label(Label("//hello/world:world")) == Label("//hello/world:world"))

    # relative labels
    for (actual, expected) in ctx.attr.relative_asserts.items():
        asserts.true(env, actual.label == Label(expected))

    return unittest.end(env)

def _is_external_label_test_impl(ctx):
    env = unittest.begin(ctx)

    # assert that labels and strings that are constructed within this workspace return false
    asserts.false(env, utils.is_external_label("//some/label"))
    asserts.false(env, utils.is_external_label(Label("//some/label")))
    asserts.false(env, utils.is_external_label(Label("@aspect_bazel_lib//some/label")))
    asserts.false(env, ctx.attr.internal_with_workspace_as_string)

    # assert that labels and string that give a workspace return true
    asserts.true(env, utils.is_external_label(Label("@foo//some/label")))
    asserts.true(env, ctx.attr.external_as_string)

    return unittest.end(env)

def _propagate_well_known_tags_test_impl(ctx):
    env = unittest.begin(ctx)

    asserts.equals(env, ["manual", "cpu:12"], utils.propagate_well_known_tags(["foo", "manual", "bar", "cpu:12"]))

    return unittest.end(env)

to_label_test = unittest.make(
    _to_label_test_impl,
    attrs = {
        "relative_asserts": attr.label_keyed_string_dict(
            allow_files = True,
            mandatory = True,
        ),
    },
)

is_external_label_test = unittest.make(
    _is_external_label_test_impl,
    attrs = {
        "external_as_string": attr.bool(
            mandatory = True,
        ),
        "internal_with_workspace_as_string": attr.bool(
            mandatory = True,
        ),
    },
)

propagate_well_known_tags_test = unittest.make(
    _propagate_well_known_tags_test_impl,
)

# buildifier: disable=function-docstring
def file_exists_test():
    # Tests that must run in the loading phase
    if utils.file_exists("does-not-exist"):
        fail("does-not-exist does not exist")
    if not utils.file_exists("utils_test.bzl"):
        fail("utils_test.bzl does exist")
    if (utils.file_exists("copy_to_bin")):
        fail("copy_to_bin exists, but is a directory")

# buildifier: disable=function-docstring
def utils_test_suite():
    to_label_test(name = "to_label_tests", relative_asserts = {
        utils.to_label(":utils_test.bzl"): "//lib/tests:utils_test.bzl",
    }, timeout = "short")

    is_external_label_test(
        name = "is_external_label_tests",
        external_as_string = utils.is_external_label("@foo//some/label"),
        internal_with_workspace_as_string = utils.is_external_label("@aspect_bazel_lib//some/label"),
        timeout = "short",
    )

    propagate_well_known_tags_test(
        name = "propagate_well_known_tags_tests",
        timeout = "short",
    )

    file_exists_test()
