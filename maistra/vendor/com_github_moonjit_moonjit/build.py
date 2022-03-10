#!/usr/bin/env python3

import argparse
import os
import shutil

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--prefix")
    args = parser.parse_args()
    src_dir = os.path.dirname(os.path.realpath(__file__))
    shutil.copytree(src_dir, os.path.basename(src_dir))
    os.chdir(os.path.basename(src_dir))

    os.environ["MACOSX_DEPLOYMENT_TARGET"] = "10.6"
    os.environ["DEFAULT_CC"] = os.environ.get("CC", "")
    os.environ["TARGET_CFLAGS"] = os.environ.get("CFLAGS", "") + " -fno-function-sections -fno-data-sections"
    os.environ["TARGET_LDFLAGS"] = os.environ.get("CFLAGS", "") + " -fno-function-sections -fno-data-sections"
    os.environ["CFLAGS"] = ""
    # LuaJIT compile process build a tool `buildvm` and use it, building `buildvm` with ASAN
    # will cause LSAN detect its leak and fail the build, set exitcode to 0 to make LSAN doesn't
    # fail on it.
    os.environ["LSAN_OPTIONS"] = "exitcode=0"

    if "ENVOY_MSAN" in os.environ:
      os.environ["HOST_CFLAGS"] = "-fno-sanitize=memory"
      os.environ["HOST_LDFLAGS"] = "-fno-sanitize=memory"

    # Remove LuaJIT from ASAN for now.
    # TODO(htuch): Remove this when https://github.com/envoyproxy/envoy/issues/6084 is resolved.
    if "ENVOY_CONFIG_ASAN" in os.environ or "ENVOY_CONFIG_MSAN" in os.environ:
      os.environ["TARGET_CFLAGS"] += " -fsanitize-blacklist=%s/com_github_moonjit_moonjit/clang-asan-blocklist.txt" % os.environ["PWD"]
      with open("clang-asan-blocklist.txt", "w") as f:
        f.write("fun:*\n")

    os.system('make -j{} V=1 PREFIX="{}" install'.format(os.cpu_count(), args.prefix))

main()

