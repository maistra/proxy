nogo test with coverage
=======================

.. _nogo: /go/nogo.rst
.. _go_tool_library: /go/core.rst#_go_tool_library
.. _#1940: https://github.com/bazelbuild/rules_go/issues/1940
.. _#2146: https://github.com/bazelbuild/rules_go/issues/2146

Tests to ensure that `nogo`_ works with coverage.

coverage_test
-------------
Checks that `nogo`_ works when coverage is enabled. All covered libraries gain
an implicit dependencies on ``//go/tools/coverdata``, which is a
`go_tool_library`_, which isn't built with `nogo`_. We should be able to
handle libraries like this that do not have serialized facts. Verifies `#1940`_.

Also checks that `nogo`_ itself can be built with coverage enabled.
Verifies `#2146`_.
