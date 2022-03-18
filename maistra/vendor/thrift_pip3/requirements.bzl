# Install pip requirements.
#
# Generated from /tmp/tmp.7NDr2YZHWG/external/envoy/test/extensions/filters/network/thrift_proxy/requirements.txt

load("@rules_python//python:whl.bzl", "whl_library")

def pip_install():
  
  if "thrift_pip3_pypi__six_1_15_0" not in native.existing_rules():
    whl_library(
        name = "thrift_pip3_pypi__six_1_15_0",
        python_interpreter = "python3",
        whl = "@thrift_pip3//:six-1.15.0-py2.py3-none-any.whl",
        requirements = "@thrift_pip3//:requirements.bzl",
        extras = []
    )

  if "thrift_pip3_pypi__thrift_0_13_0" not in native.existing_rules():
    whl_library(
        name = "thrift_pip3_pypi__thrift_0_13_0",
        python_interpreter = "python3",
        whl = "@thrift_pip3//:thrift-0.13.0-py3-none-any.whl",
        requirements = "@thrift_pip3//:requirements.bzl",
        extras = ["ssl"]
    )

_requirements = {
  "six": "@thrift_pip3_pypi__six_1_15_0//:pkg","thrift": "@thrift_pip3_pypi__thrift_0_13_0//:pkg","thrift[ssl]": "@thrift_pip3_pypi__thrift_0_13_0//:ssl"
}

all_requirements = _requirements.values()

def requirement(name):
  name_key = name.replace("-", "_").lower()
  if name_key not in _requirements:
    fail("Could not find pip-provided dependency: '%s'" % name)
  return _requirements[name_key]
