Repository rules
================

.. _http_archive.strip_prefix: https://docs.bazel.build/versions/master/be/workspace.html#http_archive.strip_prefix
.. _native git_repository rule: https://docs.bazel.build/versions/master/be/workspace.html#git_repository
.. _native http_archive rule: https://docs.bazel.build/versions/master/be/workspace.html#http_archive
.. _manifest.bzl: third_party/manifest.bzl
.. _Directives: /README.rst#directives
.. _`@bazel_tools//tools/build_defs/repo:git.bzl`: https://github.com/bazelbuild/bazel/blob/master/tools/build_defs/repo/git.bzl
.. _`@bazel_tools//tools/build_defs/repo:http.bzl`: https://github.com/bazelbuild/bazel/blob/master/tools/build_defs/repo/http.bzl

.. role:: param(kbd)
.. role:: type(emphasis)
.. role:: value(code)
.. |mandatory| replace:: **mandatory value**

Repository rules are Bazel rules that can be used in WORKSPACE files to import
projects in external repositories. Repository rules may download projects
and transform them by applying patches or generating build files.

The Gazelle repository provides three rules:

* `go_repository`_ downloads a Go project using either ``go mod download``, a
  version control tool like ``git``, or a direct HTTP download. It understands
  Go import path redirection. If build files are not already present, it can
  generate them with Gazelle.
* `git_repository`_ downloads a project with git. Unlike the native
  ``git_repository``, this rule allows you to specify an "overlay": a set of
  files to be copied into the downloaded project. This may be used to add
  pre-generated build files to a project that doesn't have them.
* `http_archive`_ downloads a project via HTTP. It also lets you specify
  overlay files.

**NOTE:** ``git_repository`` and ``http_archive`` are deprecated in favor of the
rules of the same name in `@bazel_tools//tools/build_defs/repo:git.bzl`_ and
`@bazel_tools//tools/build_defs/repo:http.bzl`_.

Repository rules can be loaded and used in WORKSPACE like this:

.. code:: bzl

  load("@bazel_gazelle//:deps.bzl", "go_repository")

  go_repository(
      name = "com_github_pkg_errors",
      commit = "816c9085562cd7ee03e7f8188a1cfd942858cded",
      importpath = "github.com/pkg/errors",
  )

Gazelle can add and update some of these rules automatically using the
``update-repos`` command. For example, the rule above can be added with:

.. code::

  $ gazelle update-repos github.com/pkg/errors

go_repository
-------------

``go_repository`` downloads a Go project and generates build files with Gazelle
if they are not already present. This is the simplest way to depend on
external Go projects.

When ``go_repository`` is in module mode, it saves downloaded modules in a shared,
internal cache within Bazel's cache. It may be cleared with ``bazel clean --expunge``.
By setting the environment variable ``GO_REPOSITORY_USE_HOST_CACHE=1``, you can
force ``go_repository`` to use the module cache on the host system in the location
returned by ``go env GOPATH``.

**Example**

.. code:: bzl

  load("@bazel_gazelle//:deps.bzl", "go_repository")

  # Download using "go mod download"
  go_repository(
      name = "com_github_pkg_errors",
      importpath = "github.com/pkg/errors",
      sum = "h1:iURUrRGxPUNPdy5/HRSm+Yj6okJ6UtLINN0Q9M4+h3I=",
      version = "v0.8.1",
  )

  # Download automatically via git
  go_repository(
      name = "com_github_pkg_errors",
      commit = "816c9085562cd7ee03e7f8188a1cfd942858cded",
      importpath = "github.com/pkg/errors",
  )

  # Download from git fork
  go_repository(
      name = "com_github_pkg_errors",
      commit = "816c9085562cd7ee03e7f8188a1cfd942858cded",
      importpath = "github.com/pkg/errors",
      remote = "https://example.com/fork/github.com/pkg/errors",
      vcs = "git",
  )

  # Download via HTTP
  go_repository(
      name = "com_github_pkg_errors",
      importpath = "github.com/pkg/errors",
      urls = ["https://codeload.github.com/pkg/errors/zip/816c9085562cd7ee03e7f8188a1cfd942858cded"],
      strip_prefix = "errors-816c9085562cd7ee03e7f8188a1cfd942858cded",
      type = "zip",
  )

**Attributes**

+--------------------------------+----------------------+---------------------------------------------------------------+
| **Name**                       | **Type**             | **Default value**                                             |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`name`                  | :type:`string`       | |mandatory|                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| A unique name for this rule. This should usually be the Java-package-style                                            |
| name of the URL, with underscores as separators, for example,                                                         |
| ``com_github_example_project``.                                                                                       |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`importpath`            | :type:`string`       | |mandatory|                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| The Go import path that matches the root directory of this repository. In                                             |
| module mode (when ``version`` is set), this must be the module path. If                                               |
| neither ``urls`` nor ``remote`` is specified, ``go_repository`` will                                                  |
| automatically find the true path of the module, applying import path                                                  |
| redirection.                                                                                                          |
|                                                                                                                       |
| If build files are generated for this repository, libraries will have their                                           |
| ``importpath`` attributes prefixed with this ``importpath`` string.                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`version`               | :type:`string`       | :value:`""`                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| If specified, ``go_repository`` will download the module at this version                                              |
| using ``go mod download``. ``sum`` must also be set. ``commit``, ``tag``,                                             |
| and ``urls`` may not be set.                                                                                          |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`sum`                   | :type:`string`       | :value:`""`                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| A hash of the module contents. In module mode, ``go_repository`` will verify                                          |
| the downloaded module matches this sum. May only be set when ``version``                                              |
| is also set.                                                                                                          |
|                                                                                                                       |
| A value for ``sum`` may be found in the ``go.sum`` file or by running                                                 |
| ``go mod download -json <module>@<version>``.                                                                         |
+-----------------------------------+----------------------+------------------------------------------------------------+
| :param:`build_naming_convention`  | :type:`string`       | :value:`""`                                                |
+--------------------------------+----------------------+---------------------------------------------------------------+
| Sets the library naming convention to use when resolving dependencies against this external                           |
| repository. If unset, the convention from the external workspace is used.                                             |
| Legal values are ``go_default_library``, ``import``, and ``import_alias``.                                            |
|                                                                                                                       |
| See ``-go_naming_convention`` for more information.                                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`replace`               | :type:`string`       | :value:`""`                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| A replacement for the module named by ``importpath``. The module named by                                             |
| ``replace`` will be downloaded at ``version`` and verified with ``sum``.                                              |
|                                                                                                                       |
| NOTE: There is no ``go_repository`` equivalent to file path ``replace``                                               |
| directives. Use ``local_repository`` instead.                                                                         |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`commit`                | :type:`string`       | :value:`""`                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| If the repository is downloaded using a version control tool, this is the                                             |
| commit or revision to check out. With git, this would be a sha1 commit id.                                            |
| ``commit`` and ``tag`` may not both be set.                                                                           |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`tag`                   | :type:`string`       | :value:`""`                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| If the repository is downloaded using a version control tool, this is the                                             |
| named revision to check out. ``commit`` and ``tag`` may not both be set.                                              |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`vcs`                   | :type:`string`       | :value:`""`                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| One of ``"git"``, ``"hg"``, ``"svn"``, ``"bzr"``.                                                                     |
|                                                                                                                       |
| The version control system to use. This is usually determined automatically,                                          |
| but it may be necessary to set this when ``remote`` is set and the VCS cannot                                         |
| be inferred. You must have the corresponding tool installed on your host.                                             |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`remote`                | :type:`string`       | :value:`""`                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| The VCS location where the repository should be downloaded from. This is                                              |
| usually inferred from ``importpath``, but you can set ``remote`` to download                                          |
| from a private repository or a fork.                                                                                  |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`urls`                  | :type:`string list`  | :value:`[]`                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| A list of HTTP(S) URLs where an archive containing the project can be                                                 |
| downloaded. Bazel will attempt to download from the first URL; the others                                             |
| are mirrors.                                                                                                          |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`strip_prefix`          | :type:`string`       | :value:`""`                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| If the repository is downloaded via HTTP (``urls`` is set), this is a                                                 |
| directory prefix to strip. See `http_archive.strip_prefix`_.                                                          |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`type`                  | :type:`string`       | :value:`""`                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| One of ``"zip"``, ``"tar.gz"``, ``"tgz"``, ``"tar.bz2"``, ``"tar.xz"``.                                               |
|                                                                                                                       |
| If the repository is downloaded via HTTP (``urls`` is set), this is the                                               |
| file format of the repository archive. This is normally inferred from the                                             |
| downloaded file name.                                                                                                 |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`sha256`                | :type:`string`       | :value:`""`                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| If the repository is downloaded via HTTP (``urls`` is set), this is the                                               |
| SHA-256 sum of the downloaded archive. When set, Bazel will verify the archive                                        |
| against this sum before extracting it.                                                                                |
|                                                                                                                       |
| **CAUTION:** Do not use this with services that prepare source archives on                                            |
| demand, such as codeload.github.com. Any minor change in the server software                                          |
| can cause differences in file order, alignment, and compression that break                                            |
| SHA-256 sums.                                                                                                         |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`build_file_generation` | :type:`string`       | :value:`"auto"`                                               |
+--------------------------------+----------------------+---------------------------------------------------------------+
| One of ``"auto"``, ``"on"``, ``"off"``.                                                                               |
|                                                                                                                       |
| Whether Gazelle should generate build files in the repository. In ``"auto"``                                          |
| mode, Gazelle will run if there is no build file in the repository root                                               |
| directory.                                                                                                            |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`build_config`          | :type:`label`        | :value:`@bazel_gazelle_go_repository_config//:WORKSPACE`      |
+--------------------------------+----------------------+---------------------------------------------------------------+
| A file that Gazelle should read to learn about external repositories before                                           |
| generating build files. This is useful for dependency resolution. For example,                                        |
| a ``go_repository`` rule in this file establishes a mapping between a                                                 |
| repository name like ``golang.org/x/tools`` and a workspace name like                                                 |
| ``org_golang_x_tools``. Workspace directives like                                                                     |
| ``# gazelle:repository_macro`` are recognized.                                                                        |
|                                                                                                                       |
| ``go_repository`` rules will be re-evaluated when parts of WORKSPACE related                                          |
| to Gazelle's configuration are changed, including Gazelle directives and                                              |
| ``go_repository`` ``name`` and ``importpath`` attributes.                                                             |
| Their content should still be fetched from a local cache, but build files                                             |
| will be regenerated. If this is not desirable, ``build_config`` may be set                                            |
| to a less frequently updated file or ``None`` to disable this functionality.                                          |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`build_file_name`       | :type:`string`       | :value:`BUILD.bazel,BUILD`                                    |
+--------------------------------+----------------------+---------------------------------------------------------------+
| Comma-separated list of names Gazelle will consider to be build files.                                                |
| If a repository contains files named ``build`` that aren't related to Bazel,                                          |
| it may help to set this to ``"BUILD.bazel"``, especially on case-insensitive                                          |
| file systems.                                                                                                         |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`build_external`        | :type:`string`       | :value:`""`                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| One of ``"external"``, ``"vendored"``.                                                                                |
|                                                                                                                       |
| This sets Gazelle's ``-external`` command line flag.                                                                  |
|                                                                                                                       |
| **NOTE:** This cannot be used to ignore the ``vendor`` directory in a                                                 |
| repository. The ``-external`` flag only controls how Gazelle resolves                                                 |
| imports which are not present in the repository. Use                                                                  |
| ``build_extra_args = ["-exclude=vendor"]`` instead.                                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`build_tags`            | :type:`string list`  | :value:`[]`                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| This sets Gazelle's ``-build_tags`` command line flag.                                                                |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`build_file_proto_mode` | :type:`string`       | :value:`""`                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| One of ``"default"``, ``"legacy"``, ``"disable"``, ``"disable_global"`` or                                            |
| ``"package"``.                                                                                                        |
|                                                                                                                       |
| This sets Gazelle's ``-proto`` command line flag. See Directives_ for more                                            |
| information on each mode.                                                                                             |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`build_extra_args`      | :type:`string list`  | :value:`[]`                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| A list of additional command line arguments to pass to Gazelle when                                                   |
| generating build files.                                                                                               |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`build_directives`      | :type:`string list`  | :value:`[]`                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| A list of directives to be written to the root level build file before                                                |
| Calling Gazelle to generate build files. Each string in the list will be                                              |
| prefixed with `#` automatically. A common use case is to pass a list of                                               |
| Gazelle directives.                                                                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`patches`               | :type:`label list`   | :value:`[]`                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| A list of patches to apply to the repository after gazelle runs.                                                      |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`patch_tool`            | :type:`string`       | :value:`"patch"`                                              |
+--------------------------------+----------------------+---------------------------------------------------------------+
| The patch tool used to apply ``patches``.                                                                             |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`patch_args`            | :type:`string list`  | :value:`["-p0"]`                                              |
+--------------------------------+----------------------+---------------------------------------------------------------+
| Arguments passed to the patch tool when applying patches.                                                             |
+--------------------------------+----------------------+---------------------------------------------------------------+
| :param:`patch_cmds`            | :type:`string list`  | :value:`[]`                                                   |
+--------------------------------+----------------------+---------------------------------------------------------------+
| Commands to run in the repository after patches are applied.                                                          |
+--------------------------------+----------------------+---------------------------------------------------------------+

git_repository
--------------

**NOTE:** ``git_repository`` is deprecated in favor of the rule of the same name
in `@bazel_tools//tools/build_defs/repo:git.bzl`_.

``git_repository`` downloads a project with git. It has the same features as the
`native git_repository rule`_, but it also allows you to copy a set of files
into the repository after download. This is particularly useful for placing
pre-generated build files.

**Example**

.. code:: bzl

  load("@bazel_gazelle//:deps.bzl", "git_repository")

  git_repository(
      name = "com_github_pkg_errors",
      remote = "https://github.com/pkg/errors",
      commit = "816c9085562cd7ee03e7f8188a1cfd942858cded",
      overlay = {
          "@my_repo//third_party:com_github_pkg_errors/BUILD.bazel.in" : "BUILD.bazel",
      },
  )

**Attributes**

+--------------------------------+----------------------+-------------------------------------------------+
| **Name**                       | **Type**             | **Default value**                               |
+--------------------------------+----------------------+-------------------------------------------------+
| :param:`name`                  | :type:`string`       | |mandatory|                                     |
+--------------------------------+----------------------+-------------------------------------------------+
| A unique name for this rule. This should usually be the Java-package-style                              |
| name of the URL, with underscores as separators, for example,                                           |
| ``com_github_example_project``.                                                                         |
+--------------------------------+----------------------+-------------------------------------------------+
| :param:`remote`                | :type:`string`       | |mandatory|                                     |
+--------------------------------+----------------------+-------------------------------------------------+
| The remote repository to download.                                                                      |
+--------------------------------+----------------------+-------------------------------------------------+
| :param:`commit`                | :type:`string`       | :value:`""`                                     |
+--------------------------------+----------------------+-------------------------------------------------+
| The git commit to check out. Either ``commit`` or ``tag`` may be specified.                             |
+--------------------------------+----------------------+-------------------------------------------------+
| :param:`tag`                   | :type:`tag`          | :value:`""`                                     |
+--------------------------------+----------------------+-------------------------------------------------+
| The git tag to check out. Either ``commit`` or ``tag`` may be specified.                                |
+--------------------------------+----------------------+-------------------------------------------------+
| :param:`overlay`               | :type:`dict`         | :value:`{}`                                     |
+--------------------------------+----------------------+-------------------------------------------------+
| A set of files to copy into the downloaded repository. The keys in this                                 |
| dictionary are Bazel labels that point to the files to copy. These must be                              |
| fully qualified labels (i.e., ``@repo//pkg:name``) because relative labels                              |
| are interpreted in the checked out repository, not the repository containing                            |
| the WORKSPACE file. The values in this dictionary are root-relative paths                               |
| where the overlay files should be written.                                                              |
|                                                                                                         |
| It's convenient to store the overlay dictionaries for all repositories in                               |
| a separate .bzl file. See Gazelle's `manifest.bzl`_ for an example.                                     |
+--------------------------------+----------------------+-------------------------------------------------+

http_archive
------------

**NOTE:** ``http_archive`` is deprecated in favor of the rule of the same name
in `@bazel_tools//tools/build_defs/repo:http.bzl`_.

``http_archive`` downloads a project over HTTP(S). It has the same features as
the `native http_archive rule`_, but it also allows you to copy a set of files
into the repository after download. This is particularly useful for placing
pre-generated build files.

**Example**

.. code:: bzl

  load("@bazel_gazelle//:deps.bzl", "http_archive")

  http_archive(
      name = "com_github_pkg_errors",
      urls = ["https://codeload.github.com/pkg/errors/zip/816c9085562cd7ee03e7f8188a1cfd942858cded"],
      strip_prefix = "errors-816c9085562cd7ee03e7f8188a1cfd942858cded",
      type = "zip",
      overlay = {
          "@my_repo//third_party:com_github_pkg_errors/BUILD.bazel.in" : "BUILD.bazel",
      },
  )

**Attributes**

+--------------------------------+----------------------+-------------------------------------------------+
| **Name**                       | **Type**             | **Default value**                               |
+--------------------------------+----------------------+-------------------------------------------------+
| :param:`name`                  | :type:`string`       | |mandatory|                                     |
+--------------------------------+----------------------+-------------------------------------------------+
| A unique name for this rule. This should usually be the Java-package-style                              |
| name of the URL, with underscores as separators, for example,                                           |
| ``com_github_example_project``.                                                                         |
+--------------------------------+----------------------+-------------------------------------------------+
| :param:`urls`                  | :type:`string list`  | |mandatory|                                     |
+--------------------------------+----------------------+-------------------------------------------------+
| A list of HTTP(S) URLs where the project can be downloaded. Bazel will                                  |
| attempt to download the first URL; the others are mirrors.                                              |
+--------------------------------+----------------------+-------------------------------------------------+
| :param:`sha256`                | :type:`string`       | :value:`""`                                     |
+--------------------------------+----------------------+-------------------------------------------------+
| The SHA-256 sum of the downloaded archive. When set, Bazel will verify the                              |
| archive against this sum before extracting it.                                                          |
|                                                                                                         |
| **CAUTION:** Do not use this with services that prepare source archives on                              |
| demand, such as codeload.github.com. Any minor change in the server software                            |
| can cause differences in file order, alignment, and compression that break                              |
| SHA-256 sums.                                                                                           |
+--------------------------------+----------------------+-------------------------------------------------+
| :param:`strip_prefix`          | :type:`string`       | :value:`""`                                     |
+--------------------------------+----------------------+-------------------------------------------------+
| A directory prefix to strip. See `http_archive.strip_prefix`_.                                          |
+--------------------------------+----------------------+-------------------------------------------------+
| :param:`type`                  | :type:`string`       | :value:`""`                                     |
+--------------------------------+----------------------+-------------------------------------------------+
| One of ``"zip"``, ``"tar.gz"``, ``"tgz"``, ``"tar.bz2"``, ``"tar.xz"``.                                 |
|                                                                                                         |
| The file format of the repository archive. This is normally inferred from                               |
| the downloaded file name.                                                                               |
+--------------------------------+----------------------+-------------------------------------------------+
| :param:`overlay`               | :type:`dict`         | :value:`{}`                                     |
+--------------------------------+----------------------+-------------------------------------------------+
| A set of files to copy into the downloaded repository. The keys in this                                 |
| dictionary are Bazel labels that point to the files to copy. These must be                              |
| fully qualified labels (i.e., ``@repo//pkg:name``) because relative labels                              |
| are interpreted in the checked out repository, not the repository containing                            |
| the WORKSPACE file. The values in this dictionary are root-relative paths                               |
| where the overlay files should be written.                                                              |
|                                                                                                         |
| It's convenient to store the overlay dictionaries for all repositories in                               |
| a separate .bzl file. See Gazelle's `manifest.bzl`_ for an example.                                     |
+--------------------------------+----------------------+-------------------------------------------------+
