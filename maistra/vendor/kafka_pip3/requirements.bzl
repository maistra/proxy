# Install pip requirements.
#
# Generated from /tmp/tmp.EGcLX7EDYl/external/envoy/source/extensions/filters/network/kafka/requirements.txt

load("@rules_python//python:whl.bzl", "whl_library")

def pip_install():
  
  if "kafka_pip3_pypi__Jinja2_2_11_2" not in native.existing_rules():
    whl_library(
        name = "kafka_pip3_pypi__Jinja2_2_11_2",
        python_interpreter = "python3",
        whl = "@kafka_pip3//:Jinja2-2.11.2-py2.py3-none-any.whl",
        requirements = "@kafka_pip3//:requirements.bzl",
        extras = []
    )

  if "kafka_pip3_pypi__MarkupSafe_1_1_1" not in native.existing_rules():
    whl_library(
        name = "kafka_pip3_pypi__MarkupSafe_1_1_1",
        python_interpreter = "python3",
        whl = "@kafka_pip3//:MarkupSafe-1.1.1-cp36-cp36m-manylinux1_x86_64.whl",
        requirements = "@kafka_pip3//:requirements.bzl",
        extras = []
    )

_requirements = {
  "jinja2": "@kafka_pip3_pypi__Jinja2_2_11_2//:pkg","markupsafe": "@kafka_pip3_pypi__MarkupSafe_1_1_1//:pkg"
}

all_requirements = _requirements.values()

def requirement(name):
  name_key = name.replace("-", "_").lower()
  if name_key not in _requirements:
    fail("Could not find pip-provided dependency: '%s'" % name)
  return _requirements[name_key]
