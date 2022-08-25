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

go_exts = [
    ".go",
]

asm_exts = [
    ".s",
    ".S",
    ".h",  # may be included by .s
]

# be consistent to cc_library.
hdr_exts = [
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".inc",
]

c_exts = [
    ".c",
    ".h",
]

cxx_exts = [
    ".cc",
    ".cxx",
    ".cpp",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
]

objc_exts = [
    ".m",
    ".mm",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
]

cgo_exts = [
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".inc",
    ".m",
    ".mm",
]

def pkg_dir(workspace_root, package_name):
    """Returns a path to a package directory from the root of the sandbox."""
    if workspace_root and package_name:
        return workspace_root + "/" + package_name
    if workspace_root:
        return workspace_root
    if package_name:
        return package_name
    return "."

def split_srcs(srcs):
    """Returns a struct of sources, divided by extension."""
    sources = struct(
        go = [],
        asm = [],
        headers = [],
        c = [],
        cxx = [],
        objc = [],
    )
    ext_pairs = (
        (sources.go, go_exts),
        (sources.headers, hdr_exts),
        (sources.asm, asm_exts),
        (sources.c, c_exts),
        (sources.cxx, cxx_exts),
        (sources.objc, objc_exts),
    )
    extmap = {}
    for outs, exts in ext_pairs:
        for ext in exts:
            ext = ext[1:]  # strip the dot
            if ext in extmap:
                break
            extmap[ext] = outs
    for src in as_iterable(srcs):
        extouts = extmap.get(src.extension)
        if extouts == None:
            fail("Unknown source type {0}".format(src.basename))
        extouts.append(src)
    return sources

def join_srcs(source):
    """Combines source from a split_srcs struct into a single list."""
    return source.go + source.headers + source.asm + source.c + source.cxx + source.objc

def env_execute(ctx, arguments, environment = {}, **kwargs):
    """Executes a command in for a repository rule.

    It prepends "env -i" to "arguments" before calling "ctx.execute".
    Variables that aren't explicitly mentioned in "environment"
    are removed from the environment. This should be preferred to "ctx.execute"
    in most situations.
    """
    if ctx.os.name.startswith("windows"):
        return ctx.execute(arguments, environment = environment, **kwargs)
    env_args = ["env", "-i"]
    environment = dict(environment)
    for var in ["TMP", "TMPDIR"]:
        if var in ctx.os.environ and not var in environment:
            environment[var] = ctx.os.environ[var]
    for k, v in environment.items():
        env_args.append("%s=%s" % (k, v))
    arguments = env_args + arguments
    return ctx.execute(arguments, **kwargs)

def os_path(ctx, path):
    path = str(path)  # maybe convert from path type
    if ctx.os.name.startswith("windows"):
        path = path.replace("/", "\\")
    return path

def executable_path(ctx, path):
    path = os_path(ctx, path)
    if ctx.os.name.startswith("windows"):
        path += ".exe"
    return path

def executable_extension(ctx):
    extension = ""
    if ctx.os.name.startswith("windows"):
        extension = ".exe"
    return extension

def goos_to_extension(goos):
    if goos == "windows":
        return ".exe"
    return ""

ARCHIVE_EXTENSION = ".a"

SHARED_LIB_EXTENSIONS = [".dll", ".dylib", ".so"]

def goos_to_shared_extension(goos):
    return {
        "windows": ".dll",
        "darwin": ".dylib",
    }.get(goos, ".so")

def has_shared_lib_extension(path):
    """
    Matches filenames of shared libraries, with or without a version number extension.
    """
    return (has_simple_shared_lib_extension(path) or
            get_versioned_shared_lib_extension(path))

def has_simple_shared_lib_extension(path):
    """
    Matches filenames of shared libraries, without a version number extension.
    """
    return any([path.endswith(ext) for ext in SHARED_LIB_EXTENSIONS])

def get_versioned_shared_lib_extension(path):
    """If appears to be an versioned .so or .dylib file, return the extension; otherwise empty"""
    parts = path.split("/")[-1].split(".")
    if not parts[-1].isdigit():
        return ""

    # only iterating to 1 because parts[0] has to be the lib name
    for i in range(len(parts) - 1, 0, -1):
        if not parts[i].isdigit():
            if parts[i] == "dylib" or parts[i] == "so":
                return ".".join(parts[i:])

            # something like foo.bar.1.2 or dylib.1.2
            return ""

    # something like 1.2.3, or so.1.2, or dylib.1.2, or foo.1.2
    return ""

MINIMUM_BAZEL_VERSION = "4.2.1"

def as_list(v):
    """Returns a list, tuple, or depset as a list."""
    if type(v) == "list":
        return v
    if type(v) == "tuple":
        return list(v)
    if type(v) == "depset":
        return v.to_list()
    fail("as_list failed on {}".format(v))

def as_iterable(v):
    """Returns a list, tuple, or depset as something iterable."""
    if type(v) == "list":
        return v
    if type(v) == "tuple":
        return v
    if type(v) == "depset":
        return v.to_list()
    fail("as_iterator failed on {}".format(v))

def as_tuple(v):
    """Returns a list, tuple, or depset as a tuple."""
    if type(v) == "tuple":
        return v
    if type(v) == "list":
        return tuple(v)
    if type(v) == "depset":
        return tuple(v.to_list())
    fail("as_tuple failed on {}".format(v))

def as_set(v):
    """Returns a list, tuple, or depset as a depset."""
    if type(v) == "depset":
        return v
    if type(v) == "list":
        return depset(v)
    if type(v) == "tuple":
        return depset(v)
    fail("as_tuple failed on {}".format(v))
