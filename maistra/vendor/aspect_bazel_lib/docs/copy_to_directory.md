<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Copy files and directories to an output directory.


<a id="copy_to_directory"></a>

## copy_to_directory

<pre>
copy_to_directory(<a href="#copy_to_directory-name">name</a>, <a href="#copy_to_directory-allow_overwrites">allow_overwrites</a>, <a href="#copy_to_directory-exclude_prefixes">exclude_prefixes</a>, <a href="#copy_to_directory-exclude_srcs_packages">exclude_srcs_packages</a>,
                  <a href="#copy_to_directory-exclude_srcs_patterns">exclude_srcs_patterns</a>, <a href="#copy_to_directory-hardlink">hardlink</a>, <a href="#copy_to_directory-include_external_repositories">include_external_repositories</a>,
                  <a href="#copy_to_directory-include_srcs_packages">include_srcs_packages</a>, <a href="#copy_to_directory-include_srcs_patterns">include_srcs_patterns</a>, <a href="#copy_to_directory-out">out</a>, <a href="#copy_to_directory-replace_prefixes">replace_prefixes</a>, <a href="#copy_to_directory-root_paths">root_paths</a>,
                  <a href="#copy_to_directory-srcs">srcs</a>, <a href="#copy_to_directory-verbose">verbose</a>)
</pre>

Copies files and directories to an output directory.

Files and directories can be arranged as needed in the output directory using
the `root_paths`, `include_srcs_patterns`, `exclude_srcs_patterns` and `replace_prefixes` attributes.

Filters and transformations are applied in the following order:

1. `include_external_repositories`

2. `include_srcs_packages`

3. `exclude_srcs_packages`

4. `root_paths`

5. `include_srcs_patterns`

6. `exclude_srcs_patterns`

7. `replace_prefixes`

For more information each filters / transformations applied, see
the documentation for the specific filter / transformation attribute.


Glob patterns are supported. Standard wildcards (globbing patterns) plus the `**` doublestar (aka. super-asterisk)
are supported with the underlying globbing library, https://github.com/bmatcuk/doublestar. This is the same
globbing library used by [gazelle](https://github.com/bazelbuild/bazel-gazelle). See https://github.com/bmatcuk/doublestar#patterns
for more information on supported globbing patterns.



**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="copy_to_directory-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/docs/build-ref.html#name">Name</a> | required |  |
| <a id="copy_to_directory-allow_overwrites"></a>allow_overwrites |  If True, allow files to be overwritten if the same output file is copied to twice.<br><br>The order of srcs matters as the last copy of a particular file will win when overwriting. Performance of copy_to_directory will be slightly degraded when allow_overwrites is True since copies cannot be parallelized out as they are calculated. Instead all copy paths must be calculated before any copies can be started.   | Boolean | optional | False |
| <a id="copy_to_directory-exclude_prefixes"></a>exclude_prefixes |  List of path prefixes (with glob support) to exclude from output directory.<br><br>DEPRECATED: use <code>exclude_srcs_patterns</code> instead<br><br>Files in srcs are not copied to the output directory if their output directory path, after applying <code>root_paths</code>, starts with or fully matches one of the patterns specified.<br><br>Forward slashes (<code>/</code>) should be used as path separators.<br><br>Files that do not have matching output directory paths are subject to subsequent filters and transformations to determine if they are copied and what their path in the output directory will be.<br><br>Globs are supported (see rule docstring above).   | List of strings | optional | [] |
| <a id="copy_to_directory-exclude_srcs_packages"></a>exclude_srcs_packages |  List of Bazel packages (with glob support) to exclude from output directory.<br><br>Files in srcs are not copied to the output directory if the Bazel package of the file matches one of the patterns specified.<br><br>Forward slashes (<code>/</code>) should be used as path separators. A first character of <code>"."</code> will be replaced by the target's package path.<br><br>Files that have do not have matching Bazel packages are subject to subsequent filters and transformations to determine if they are copied and what their path in the output directory will be.<br><br>Globs are supported (see rule docstring above).   | List of strings | optional | [] |
| <a id="copy_to_directory-exclude_srcs_patterns"></a>exclude_srcs_patterns |  List of paths (with glob support) to exclude from output directory.<br><br>Files in srcs are not copied to the output directory if their output directory path, after applying <code>root_paths</code>, matches one of the patterns specified.<br><br>Forward slashes (<code>/</code>) should be used as path separators.<br><br>Files that do not have matching output directory paths are subject to subsequent filters and transformations to determine if they are copied and what their path in the output directory will be.<br><br>Globs are supported (see rule docstring above).   | List of strings | optional | [] |
| <a id="copy_to_directory-hardlink"></a>hardlink |  Controls when to use hardlinks to files instead of making copies.<br><br>Creating hardlinks is much faster than making copies of files with the caveat that hardlinks share file permissions with their source.<br><br>Since Bazel removes write permissions on files in the output tree after an action completes, hardlinks to source files are not recommended since write permissions will be inadvertently removed from sources files.<br><br>- <code>auto</code>: hardlinks are used for generated files already in the output tree - <code>off</code>: all files are copied - <code>on</code>: hardlinks are used for all files (not recommended)   | String | optional | "auto" |
| <a id="copy_to_directory-include_external_repositories"></a>include_external_repositories |  List of external repository names (with glob support) to include in the output directory.<br><br>Files from external repositories are only copied into the output directory if the external repository they come from matches one of the external repository patterns specified.<br><br>When copied from an external repository, the file path in the output directory defaults to the file's path within the external repository. The external repository name is _not_ included in that path.<br><br>For example, the following copies <code>@external_repo//path/to:file</code> to <code>path/to/file</code> within the output directory.<br><br><pre><code> copy_to_directory(     name = "dir",     include_external_repositories = ["external_*"],     srcs = ["@external_repo//path/to:file"], ) </code></pre><br><br>Files that come from matching external are subject to subsequent filters and transformations to determine if they are copied and what their path in the output directory will be. The external repository name of the file from an external repository is not included in the output directory path and is considered in subsequent filters and transformations.<br><br>Globs are supported (see rule docstring above).   | List of strings | optional | [] |
| <a id="copy_to_directory-include_srcs_packages"></a>include_srcs_packages |  List of Bazel packages (with glob support) to include in output directory.<br><br>Files in srcs are only copied to the output directory if the Bazel package of the file matches one of the patterns specified.<br><br>Forward slashes (<code>/</code>) should be used as path separators. A first character of <code>"."</code> will be replaced by the target's package path.<br><br>Defaults to <code>["**"]</code> which includes sources from all packages.<br><br>Files that have matching Bazel packages are subject to subsequent filters and transformations to determine if they are copied and what their path in the output directory will be.<br><br>Globs are supported (see rule docstring above).   | List of strings | optional | ["**"] |
| <a id="copy_to_directory-include_srcs_patterns"></a>include_srcs_patterns |  List of paths (with glob support) to include in output directory.<br><br>Files in srcs are only copied to the output directory if their output directory path, after applying <code>root_paths</code>, matches one of the patterns specified.<br><br>Forward slashes (<code>/</code>) should be used as path separators.<br><br>Defaults to <code>["**"]</code> which includes all sources.<br><br>Files that have matching output directory paths are subject to subsequent filters and transformations to determine if they are copied and what their path in the output directory will be.<br><br>Globs are supported (see rule docstring above).   | List of strings | optional | ["**"] |
| <a id="copy_to_directory-out"></a>out |  Path of the output directory, relative to this package.<br><br>If not set, the name of the target is used.   | String | optional | "" |
| <a id="copy_to_directory-replace_prefixes"></a>replace_prefixes |  Map of paths prefixes (with glob support) to replace in the output directory path when copying files.<br><br>If the output directory path for a file starts with or fully matches a a key in the dict then the matching portion of the output directory path is replaced with the dict value for that key. The final path segment matched can be a partial match of that segment and only the matching portion will be replaced. If there are multiple keys that match, the longest match wins.<br><br>Forward slashes (<code>/</code>) should be used as path separators.<br><br>Replace prefix transformation are the final step in the list of filters and transformations. The final output path of a file being copied into the output directory is determined at this step.<br><br>Globs are supported (see rule docstring above).   | <a href="https://bazel.build/docs/skylark/lib/dict.html">Dictionary: String -> String</a> | optional | {} |
| <a id="copy_to_directory-root_paths"></a>root_paths |  List of paths (with glob support) that are roots in the output directory.<br><br>If any parent directory of a file being copied matches one of the root paths patterns specified, the output directory path will be the path relative to the root path instead of the path relative to the file's workspace. If there are multiple root paths that match, the longest match wins.<br><br>Matching is done on the parent directory of the output file path so a trailing '**' glob patterm will match only up to the last path segment of the dirname and will not include the basename. Only complete path segments are matched. Partial matches on the last segment of the root path are ignored.<br><br>Forward slashes (<code>/</code>) should be used as path separators.<br><br>A <code>"."</code> value expands to the target's package path (<code>ctx.label.package</code>).<br><br>Defaults to <code>["."]</code> which results in the output directory path of files in the target's package and and sub-packages are relative to the target's package and files outside of that retain their full workspace relative paths.<br><br>Globs are supported (see rule docstring above).   | List of strings | optional | ["."] |
| <a id="copy_to_directory-srcs"></a>srcs |  Files and/or directories or targets that provide <code>DirectoryPathInfo</code> to copy into the output directory.   | <a href="https://bazel.build/docs/build-ref.html#labels">List of labels</a> | optional | [] |
| <a id="copy_to_directory-verbose"></a>verbose |  If true, prints out verbose logs to stdout   | Boolean | optional | False |


<a id="copy_to_directory_action"></a>

## copy_to_directory_action

<pre>
copy_to_directory_action(<a href="#copy_to_directory_action-ctx">ctx</a>, <a href="#copy_to_directory_action-srcs">srcs</a>, <a href="#copy_to_directory_action-dst">dst</a>, <a href="#copy_to_directory_action-additional_files">additional_files</a>, <a href="#copy_to_directory_action-root_paths">root_paths</a>,
                         <a href="#copy_to_directory_action-include_external_repositories">include_external_repositories</a>, <a href="#copy_to_directory_action-include_srcs_packages">include_srcs_packages</a>, <a href="#copy_to_directory_action-exclude_srcs_packages">exclude_srcs_packages</a>,
                         <a href="#copy_to_directory_action-include_srcs_patterns">include_srcs_patterns</a>, <a href="#copy_to_directory_action-exclude_srcs_patterns">exclude_srcs_patterns</a>, <a href="#copy_to_directory_action-exclude_prefixes">exclude_prefixes</a>,
                         <a href="#copy_to_directory_action-replace_prefixes">replace_prefixes</a>, <a href="#copy_to_directory_action-allow_overwrites">allow_overwrites</a>, <a href="#copy_to_directory_action-is_windows">is_windows</a>)
</pre>

Legacy factory function to copy files to a directory.

This helper calculates copy paths in Starlark during analysis and performs the copies in a
bash/bat script. For improved analysis and runtime performance, it is recommended the switch
to `copy_to_directory_bin_action` which calculates copy paths and performs copies with a tool
binary, typically the `@aspect_bazel_lib//tools/copy_to_directory` `go_binary` either built
from source or provided by a toolchain.

This helper is used by copy_to_directory. It is exposed as a public API so it can be used within
other rule implementations where additional_files can also be passed in.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="copy_to_directory_action-ctx"></a>ctx |  The rule context.   |  none |
| <a id="copy_to_directory_action-srcs"></a>srcs |  Files and/or directories or targets that provide <code>DirectoryPathInfo</code> to copy into the output directory.   |  none |
| <a id="copy_to_directory_action-dst"></a>dst |  The directory to copy to. Must be a TreeArtifact.   |  none |
| <a id="copy_to_directory_action-additional_files"></a>additional_files |  List or depset of additional files to copy that are not in the <code>DefaultInfo</code> or <code>DirectoryPathInfo</code> of srcs   |  <code>[]</code> |
| <a id="copy_to_directory_action-root_paths"></a>root_paths |  List of paths that are roots in the output directory.<br><br>See copy_to_directory rule documentation for more details.   |  <code>["."]</code> |
| <a id="copy_to_directory_action-include_external_repositories"></a>include_external_repositories |  List of external repository names to include in the output directory.<br><br>See copy_to_directory rule documentation for more details.   |  <code>[]</code> |
| <a id="copy_to_directory_action-include_srcs_packages"></a>include_srcs_packages |  List of Bazel packages to include in output directory.<br><br>See copy_to_directory rule documentation for more details.   |  <code>["**"]</code> |
| <a id="copy_to_directory_action-exclude_srcs_packages"></a>exclude_srcs_packages |  List of Bazel packages (with glob support) to exclude from output directory.<br><br>See copy_to_directory rule documentation for more details.   |  <code>[]</code> |
| <a id="copy_to_directory_action-include_srcs_patterns"></a>include_srcs_patterns |  List of paths (with glob support) to include in output directory.<br><br>See copy_to_directory rule documentation for more details.   |  <code>["**"]</code> |
| <a id="copy_to_directory_action-exclude_srcs_patterns"></a>exclude_srcs_patterns |  List of paths (with glob support) to exclude from output directory.<br><br>See copy_to_directory rule documentation for more details.   |  <code>[]</code> |
| <a id="copy_to_directory_action-exclude_prefixes"></a>exclude_prefixes |  List of path prefixes to exclude from output directory.<br><br>See copy_to_directory rule documentation for more details.   |  <code>[]</code> |
| <a id="copy_to_directory_action-replace_prefixes"></a>replace_prefixes |  Map of paths prefixes to replace in the output directory path when copying files.<br><br>See copy_to_directory rule documentation for more details.   |  <code>{}</code> |
| <a id="copy_to_directory_action-allow_overwrites"></a>allow_overwrites |  If True, allow files to be overwritten if the same output file is copied to twice.<br><br>See copy_to_directory rule documentation for more details.   |  <code>False</code> |
| <a id="copy_to_directory_action-is_windows"></a>is_windows |  Deprecated and unused   |  <code>None</code> |


<a id="copy_to_directory_bin_action"></a>

## copy_to_directory_bin_action

<pre>
copy_to_directory_bin_action(<a href="#copy_to_directory_bin_action-ctx">ctx</a>, <a href="#copy_to_directory_bin_action-name">name</a>, <a href="#copy_to_directory_bin_action-dst">dst</a>, <a href="#copy_to_directory_bin_action-copy_to_directory_bin">copy_to_directory_bin</a>, <a href="#copy_to_directory_bin_action-files">files</a>, <a href="#copy_to_directory_bin_action-targets">targets</a>, <a href="#copy_to_directory_bin_action-root_paths">root_paths</a>,
                             <a href="#copy_to_directory_bin_action-include_external_repositories">include_external_repositories</a>, <a href="#copy_to_directory_bin_action-include_srcs_packages">include_srcs_packages</a>,
                             <a href="#copy_to_directory_bin_action-exclude_srcs_packages">exclude_srcs_packages</a>, <a href="#copy_to_directory_bin_action-include_srcs_patterns">include_srcs_patterns</a>, <a href="#copy_to_directory_bin_action-exclude_srcs_patterns">exclude_srcs_patterns</a>,
                             <a href="#copy_to_directory_bin_action-exclude_prefixes">exclude_prefixes</a>, <a href="#copy_to_directory_bin_action-replace_prefixes">replace_prefixes</a>, <a href="#copy_to_directory_bin_action-allow_overwrites">allow_overwrites</a>, <a href="#copy_to_directory_bin_action-hardlink">hardlink</a>, <a href="#copy_to_directory_bin_action-verbose">verbose</a>)
</pre>

Factory function to copy files to a directory using a tool binary.

The tool binary will typically be the `@aspect_bazel_lib//tools/copy_to_directory` `go_binary`
either built from source or provided by a toolchain.

This helper is used by copy_to_directory. It is exposed as a public API so it can be used within
other rule implementations where additional_files can also be passed in.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="copy_to_directory_bin_action-ctx"></a>ctx |  The rule context.   |  none |
| <a id="copy_to_directory_bin_action-name"></a>name |  Name of target creating this action used for config file generation.   |  none |
| <a id="copy_to_directory_bin_action-dst"></a>dst |  The directory to copy to. Must be a TreeArtifact.   |  none |
| <a id="copy_to_directory_bin_action-copy_to_directory_bin"></a>copy_to_directory_bin |  Copy to directory tool binary.   |  none |
| <a id="copy_to_directory_bin_action-files"></a>files |  List of files to copy into the output directory.   |  <code>[]</code> |
| <a id="copy_to_directory_bin_action-targets"></a>targets |  List of targets that provide <code>DirectoryPathInfo</code> to copy into the output directory.   |  <code>[]</code> |
| <a id="copy_to_directory_bin_action-root_paths"></a>root_paths |  List of paths that are roots in the output directory.<br><br>See copy_to_directory rule documentation for more details.   |  <code>["."]</code> |
| <a id="copy_to_directory_bin_action-include_external_repositories"></a>include_external_repositories |  List of external repository names to include in the output directory.<br><br>See copy_to_directory rule documentation for more details.   |  <code>[]</code> |
| <a id="copy_to_directory_bin_action-include_srcs_packages"></a>include_srcs_packages |  List of Bazel packages to include in output directory.<br><br>See copy_to_directory rule documentation for more details.   |  <code>["**"]</code> |
| <a id="copy_to_directory_bin_action-exclude_srcs_packages"></a>exclude_srcs_packages |  List of Bazel packages (with glob support) to exclude from output directory.<br><br>See copy_to_directory rule documentation for more details.   |  <code>[]</code> |
| <a id="copy_to_directory_bin_action-include_srcs_patterns"></a>include_srcs_patterns |  List of paths (with glob support) to include in output directory.<br><br>See copy_to_directory rule documentation for more details.   |  <code>["**"]</code> |
| <a id="copy_to_directory_bin_action-exclude_srcs_patterns"></a>exclude_srcs_patterns |  List of paths (with glob support) to exclude from output directory.<br><br>See copy_to_directory rule documentation for more details.   |  <code>[]</code> |
| <a id="copy_to_directory_bin_action-exclude_prefixes"></a>exclude_prefixes |  List of path prefixes to exclude from output directory.<br><br>See copy_to_directory rule documentation for more details.   |  <code>[]</code> |
| <a id="copy_to_directory_bin_action-replace_prefixes"></a>replace_prefixes |  Map of paths prefixes to replace in the output directory path when copying files.<br><br>See copy_to_directory rule documentation for more details.   |  <code>{}</code> |
| <a id="copy_to_directory_bin_action-allow_overwrites"></a>allow_overwrites |  If True, allow files to be overwritten if the same output file is copied to twice.<br><br>See copy_to_directory rule documentation for more details.   |  <code>False</code> |
| <a id="copy_to_directory_bin_action-hardlink"></a>hardlink |  Controls when to use hardlinks to files instead of making copies.<br><br>See copy_to_directory rule documentation for more details.   |  <code>"auto"</code> |
| <a id="copy_to_directory_bin_action-verbose"></a>verbose |  If true, prints out verbose logs to stdout   |  <code>False</code> |


<a id="copy_to_directory_lib.impl"></a>

## copy_to_directory_lib.impl

<pre>
copy_to_directory_lib.impl(<a href="#copy_to_directory_lib.impl-ctx">ctx</a>)
</pre>



**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="copy_to_directory_lib.impl-ctx"></a>ctx |  <p align="center"> - </p>   |  none |


