<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Import pip requirements into Bazel.

<a id="whl_library_alias"></a>

## whl_library_alias

<pre>
whl_library_alias(<a href="#whl_library_alias-name">name</a>, <a href="#whl_library_alias-default_version">default_version</a>, <a href="#whl_library_alias-repo_mapping">repo_mapping</a>, <a href="#whl_library_alias-version_map">version_map</a>, <a href="#whl_library_alias-wheel_name">wheel_name</a>)
</pre>



**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="whl_library_alias-name"></a>name |  A unique name for this repository.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="whl_library_alias-default_version"></a>default_version |  -   | String | required |  |
| <a id="whl_library_alias-repo_mapping"></a>repo_mapping |  A dictionary from local repository name to global repository name. This allows controls over workspace dependency resolution for dependencies of this repository.&lt;p&gt;For example, an entry <code>"@foo": "@bar"</code> declares that, for any time this repository depends on <code>@foo</code> (such as a dependency on <code>@foo//some:target</code>, it should actually resolve that dependency within globally-declared <code>@bar</code> (<code>@bar//some:target</code>).   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | required |  |
| <a id="whl_library_alias-version_map"></a>version_map |  -   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | required |  |
| <a id="whl_library_alias-wheel_name"></a>wheel_name |  -   | String | required |  |


<a id="compile_pip_requirements"></a>

## compile_pip_requirements

<pre>
compile_pip_requirements(<a href="#compile_pip_requirements-name">name</a>, <a href="#compile_pip_requirements-extra_args">extra_args</a>, <a href="#compile_pip_requirements-extra_deps">extra_deps</a>, <a href="#compile_pip_requirements-py_binary">py_binary</a>, <a href="#compile_pip_requirements-py_test">py_test</a>, <a href="#compile_pip_requirements-requirements_in">requirements_in</a>,
                         <a href="#compile_pip_requirements-requirements_txt">requirements_txt</a>, <a href="#compile_pip_requirements-requirements_darwin">requirements_darwin</a>, <a href="#compile_pip_requirements-requirements_linux">requirements_linux</a>,
                         <a href="#compile_pip_requirements-requirements_windows">requirements_windows</a>, <a href="#compile_pip_requirements-visibility">visibility</a>, <a href="#compile_pip_requirements-tags">tags</a>, <a href="#compile_pip_requirements-kwargs">kwargs</a>)
</pre>

Generates targets for managing pip dependencies with pip-compile.

By default this rules generates a filegroup named "[name]" which can be included in the data
of some other compile_pip_requirements rule that references these requirements
(e.g. with `-r ../other/requirements.txt`).

It also generates two targets for running pip-compile:

- validate with `bazel test &lt;name&gt;_test`
- update with   `bazel run &lt;name&gt;.update`


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="compile_pip_requirements-name"></a>name |  base name for generated targets, typically "requirements".   |  none |
| <a id="compile_pip_requirements-extra_args"></a>extra_args |  passed to pip-compile.   |  <code>[]</code> |
| <a id="compile_pip_requirements-extra_deps"></a>extra_deps |  extra dependencies passed to pip-compile.   |  <code>[]</code> |
| <a id="compile_pip_requirements-py_binary"></a>py_binary |  the py_binary rule to be used.   |  <code>&lt;function py_binary&gt;</code> |
| <a id="compile_pip_requirements-py_test"></a>py_test |  the py_test rule to be used.   |  <code>&lt;function py_test&gt;</code> |
| <a id="compile_pip_requirements-requirements_in"></a>requirements_in |  file expressing desired dependencies.   |  <code>None</code> |
| <a id="compile_pip_requirements-requirements_txt"></a>requirements_txt |  result of "compiling" the requirements.in file.   |  <code>None</code> |
| <a id="compile_pip_requirements-requirements_darwin"></a>requirements_darwin |  File of darwin specific resolve output to check validate if requirement.in has changes.   |  <code>None</code> |
| <a id="compile_pip_requirements-requirements_linux"></a>requirements_linux |  File of linux specific resolve output to check validate if requirement.in has changes.   |  <code>None</code> |
| <a id="compile_pip_requirements-requirements_windows"></a>requirements_windows |  File of windows specific resolve output to check validate if requirement.in has changes.   |  <code>None</code> |
| <a id="compile_pip_requirements-visibility"></a>visibility |  passed to both the _test and .update rules.   |  <code>["//visibility:private"]</code> |
| <a id="compile_pip_requirements-tags"></a>tags |  tagging attribute common to all build rules, passed to both the _test and .update rules.   |  <code>None</code> |
| <a id="compile_pip_requirements-kwargs"></a>kwargs |  other bazel attributes passed to the "_test" rule.   |  none |


<a id="multi_pip_parse"></a>

## multi_pip_parse

<pre>
multi_pip_parse(<a href="#multi_pip_parse-name">name</a>, <a href="#multi_pip_parse-default_version">default_version</a>, <a href="#multi_pip_parse-python_versions">python_versions</a>, <a href="#multi_pip_parse-python_interpreter_target">python_interpreter_target</a>,
                <a href="#multi_pip_parse-requirements_lock">requirements_lock</a>, <a href="#multi_pip_parse-kwargs">kwargs</a>)
</pre>

NOT INTENDED FOR DIRECT USE!

This is intended to be used by the multi_pip_parse implementation in the template of the
multi_toolchain_aliases repository rule.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="multi_pip_parse-name"></a>name |  the name of the multi_pip_parse repository.   |  none |
| <a id="multi_pip_parse-default_version"></a>default_version |  the default Python version.   |  none |
| <a id="multi_pip_parse-python_versions"></a>python_versions |  all Python toolchain versions currently registered.   |  none |
| <a id="multi_pip_parse-python_interpreter_target"></a>python_interpreter_target |  a dictionary which keys are Python versions and values are resolved host interpreters.   |  none |
| <a id="multi_pip_parse-requirements_lock"></a>requirements_lock |  a dictionary which keys are Python versions and values are locked requirements files.   |  none |
| <a id="multi_pip_parse-kwargs"></a>kwargs |  extra arguments passed to all wrapped pip_parse.   |  none |

**RETURNS**

The internal implementation of multi_pip_parse repository rule.


<a id="package_annotation"></a>

## package_annotation

<pre>
package_annotation(<a href="#package_annotation-additive_build_content">additive_build_content</a>, <a href="#package_annotation-copy_files">copy_files</a>, <a href="#package_annotation-copy_executables">copy_executables</a>, <a href="#package_annotation-data">data</a>, <a href="#package_annotation-data_exclude_glob">data_exclude_glob</a>,
                   <a href="#package_annotation-srcs_exclude_glob">srcs_exclude_glob</a>)
</pre>

Annotations to apply to the BUILD file content from package generated from a `pip_repository` rule.

[cf]: https://github.com/bazelbuild/bazel-skylib/blob/main/docs/copy_file_doc.md


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="package_annotation-additive_build_content"></a>additive_build_content |  Raw text to add to the generated <code>BUILD</code> file of a package.   |  <code>None</code> |
| <a id="package_annotation-copy_files"></a>copy_files |  A mapping of <code>src</code> and <code>out</code> files for [@bazel_skylib//rules:copy_file.bzl][cf]   |  <code>{}</code> |
| <a id="package_annotation-copy_executables"></a>copy_executables |  A mapping of <code>src</code> and <code>out</code> files for [@bazel_skylib//rules:copy_file.bzl][cf]. Targets generated here will also be flagged as executable.   |  <code>{}</code> |
| <a id="package_annotation-data"></a>data |  A list of labels to add as <code>data</code> dependencies to the generated <code>py_library</code> target.   |  <code>[]</code> |
| <a id="package_annotation-data_exclude_glob"></a>data_exclude_glob |  A list of exclude glob patterns to add as <code>data</code> to the generated <code>py_library</code> target.   |  <code>[]</code> |
| <a id="package_annotation-srcs_exclude_glob"></a>srcs_exclude_glob |  A list of labels to add as <code>srcs</code> to the generated <code>py_library</code> target.   |  <code>[]</code> |

**RETURNS**

str: A json encoded string of the provided content.


<a id="pip_install"></a>

## pip_install

<pre>
pip_install(<a href="#pip_install-requirements">requirements</a>, <a href="#pip_install-name">name</a>, <a href="#pip_install-kwargs">kwargs</a>)
</pre>

Accepts a locked/compiled requirements file and installs the dependencies listed within.

```python
load("@rules_python//python:pip.bzl", "pip_install")

pip_install(
    name = "pip_deps",
    requirements = ":requirements.txt",
)

load("@pip_deps//:requirements.bzl", "install_deps")

install_deps()
```


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="pip_install-requirements"></a>requirements |  A 'requirements.txt' pip requirements file.   |  <code>None</code> |
| <a id="pip_install-name"></a>name |  A unique name for the created external repository (default 'pip').   |  <code>"pip"</code> |
| <a id="pip_install-kwargs"></a>kwargs |  Additional arguments to the [<code>pip_repository</code>](./pip_repository.md) repository rule.   |  none |


<a id="pip_parse"></a>

## pip_parse

<pre>
pip_parse(<a href="#pip_parse-requirements">requirements</a>, <a href="#pip_parse-requirements_lock">requirements_lock</a>, <a href="#pip_parse-name">name</a>, <a href="#pip_parse-bzlmod">bzlmod</a>, <a href="#pip_parse-kwargs">kwargs</a>)
</pre>

Accepts a locked/compiled requirements file and installs the dependencies listed within.

Those dependencies become available in a generated `requirements.bzl` file.
You can instead check this `requirements.bzl` file into your repo, see the "vendoring" section below.

This macro wraps the [`pip_repository`](./pip_repository.md) rule that invokes `pip`.
In your WORKSPACE file:

```python
load("@rules_python//python:pip.bzl", "pip_parse")

pip_parse(
    name = "pip_deps",
    requirements_lock = ":requirements.txt",
)

load("@pip_deps//:requirements.bzl", "install_deps")

install_deps()
```

You can then reference installed dependencies from a `BUILD` file with:

```python
load("@pip_deps//:requirements.bzl", "requirement")

py_library(
    name = "bar",
    ...
    deps = [
       "//my/other:dep",
       requirement("requests"),
       requirement("numpy"),
    ],
)
```

In addition to the `requirement` macro, which is used to access the generated `py_library`
target generated from a package's wheel, The generated `requirements.bzl` file contains
functionality for exposing [entry points][whl_ep] as `py_binary` targets as well.

[whl_ep]: https://packaging.python.org/specifications/entry-points/

```python
load("@pip_deps//:requirements.bzl", "entry_point")

alias(
    name = "pip-compile",
    actual = entry_point(
        pkg = "pip-tools",
        script = "pip-compile",
    ),
)
```

Note that for packages whose name and script are the same, only the name of the package
is needed when calling the `entry_point` macro.

```python
load("@pip_deps//:requirements.bzl", "entry_point")

alias(
    name = "flake8",
    actual = entry_point("flake8"),
)
```

## Vendoring the requirements.bzl file

In some cases you may not want to generate the requirements.bzl file as a repository rule
while Bazel is fetching dependencies. For example, if you produce a reusable Bazel module
such as a ruleset, you may want to include the requirements.bzl file rather than make your users
install the WORKSPACE setup to generate it.
See https://github.com/bazelbuild/rules_python/issues/608

This is the same workflow as Gazelle, which creates `go_repository` rules with
[`update-repos`](https://github.com/bazelbuild/bazel-gazelle#update-repos)

To do this, use the "write to source file" pattern documented in
https://blog.aspect.dev/bazel-can-write-to-the-source-folder
to put a copy of the generated requirements.bzl into your project.
Then load the requirements.bzl file directly rather than from the generated repository.
See the example in rules_python/examples/pip_parse_vendored.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="pip_parse-requirements"></a>requirements |  Deprecated. See requirements_lock.   |  <code>None</code> |
| <a id="pip_parse-requirements_lock"></a>requirements_lock |  A fully resolved 'requirements.txt' pip requirement file containing the transitive set of your dependencies. If this file is passed instead of 'requirements' no resolve will take place and pip_repository will create individual repositories for each of your dependencies so that wheels are fetched/built only for the targets specified by 'build/run/test'. Note that if your lockfile is platform-dependent, you can use the <code>requirements_[platform]</code> attributes.   |  <code>None</code> |
| <a id="pip_parse-name"></a>name |  The name of the generated repository. The generated repositories containing each requirement will be of the form <code>&lt;name&gt;_&lt;requirement-name&gt;</code>.   |  <code>"pip_parsed_deps"</code> |
| <a id="pip_parse-bzlmod"></a>bzlmod |  Whether this rule is being run under a bzlmod module extension.   |  <code>False</code> |
| <a id="pip_parse-kwargs"></a>kwargs |  Additional arguments to the [<code>pip_repository</code>](./pip_repository.md) repository rule.   |  none |


