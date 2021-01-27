# Install pip requirements.
#
# Generated from /tmp/tmp.eJyz6I2o1W/external/envoy/tools/envoy_headersplit/requirements.txt

load("@rules_python//python:whl.bzl", "whl_library")

def pip_install():
  
  if "headersplit_pip3_pypi__clang_10_0_1" not in native.existing_rules():
    whl_library(
        name = "headersplit_pip3_pypi__clang_10_0_1",
        python_interpreter = "python3",
        whl = "@headersplit_pip3//:clang-10.0.1-py3-none-any.whl",
        requirements = "@headersplit_pip3//:requirements.bzl",
        extras = []
    )

_requirements = {
  "clang": "@headersplit_pip3_pypi__clang_10_0_1//:pkg"
}

all_requirements = _requirements.values()

def requirement(name):
  name_key = name.replace("-", "_").lower()
  if name_key not in _requirements:
    fail("Could not find pip-provided dependency: '%s'" % name)
  return _requirements[name_key]
