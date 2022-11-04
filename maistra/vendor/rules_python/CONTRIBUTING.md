# How to contribute

We'd love to accept your patches and contributions to this project. There are
just a few small guidelines you need to follow.

## Formatting

Starlark files should be formatted by buildifier.
We suggest using a pre-commit hook to automate this.
First [install pre-commit](https://pre-commit.com/#installation),
then run

```shell
pre-commit install
```

Otherwise the Buildkite CI will yell at you about formatting/linting violations.

## Contributor License Agreement

Contributions to this project must be accompanied by a Contributor License
Agreement. You (or your employer) retain the copyright to your contribution,
this simply gives us permission to use and redistribute your contributions as
part of the project. Head over to <https://cla.developers.google.com/> to see
your current agreements on file or to sign a new one.

You generally only need to submit a CLA once, so if you've already submitted one
(even if it was for a different project), you probably don't need to do it
again.

## Code reviews

All submissions, including submissions by project members, require review. We
use GitHub pull requests for this purpose. Consult [GitHub Help] for more
information on using pull requests.

[GitHub Help]: https://help.github.com/articles/about-pull-requests/

## Generated files

Some checked-in files are generated and need to be updated when a new PR is
merged.

### Documentation

To regenerate the content under the `docs/` directory, run this command:

```shell
bazel run //docs:update
```

This needs to be done whenever the docstrings in the corresponding .bzl files
are changed; a test failure will remind you to run this command when needed.

## Core rules

The bulk of this repo is owned and maintained by the Bazel Python community.
However, since the core Python rules (`py_binary` and friends) are still
bundled with Bazel itself, the Bazel team retains ownership of their stubs in
this repository. This will be the case at least until the Python rules are
fully migrated to Starlark code.

Practically, this means that a Bazel team member should approve any PR
concerning the core Python logic. This includes everything under the `python/`
directory except for `pip.bzl` and `requirements.txt`.

Issues should be triaged as follows:

- Anything concerning the way Bazel implements the core Python rules should be
  filed under [bazelbuild/bazel](https://github.com/bazelbuild/bazel), using
  the label `team-Rules-python`.

- If the issue specifically concerns the rules_python stubs, it should be filed
  here in this repository and use the label `core-rules`.

- Anything else, such as feature requests not related to existing core rules
  functionality, should also be filed in this repository but without the
  `core-rules` label.
