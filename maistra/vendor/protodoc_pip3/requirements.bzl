# Install pip requirements.
#
# Generated from /tmp/tmp.z85K0Ls9Cn/external/envoy/tools/protodoc/requirements.txt

load("@rules_python//python:whl.bzl", "whl_library")

def pip_install():
  
  if "protodoc_pip3_pypi__PyYAML_5_3_1" not in native.existing_rules():
    whl_library(
        name = "protodoc_pip3_pypi__PyYAML_5_3_1",
        python_interpreter = "python3",
        whl = "@protodoc_pip3//:PyYAML-5.3.1-cp36-cp36m-linux_x86_64.whl",
        requirements = "@protodoc_pip3//:requirements.bzl",
        extras = []
    )

_requirements = {
  "pyyaml": "@protodoc_pip3_pypi__PyYAML_5_3_1//:pkg"
}

all_requirements = _requirements.values()

def requirement(name):
  name_key = name.replace("-", "_").lower()
  if name_key not in _requirements:
    fail("Could not find pip-provided dependency: '%s'" % name)
  return _requirements[name_key]
