<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Rules for working with transitions.

<a id="platform_transition_binary"></a>

## platform_transition_binary

<pre>
platform_transition_binary(<a href="#platform_transition_binary-name">name</a>, <a href="#platform_transition_binary-basename">basename</a>, <a href="#platform_transition_binary-binary">binary</a>, <a href="#platform_transition_binary-target_platform">target_platform</a>)
</pre>

Transitions the binary to use the provided platform.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="platform_transition_binary-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/docs/build-ref.html#name">Name</a> | required |  |
| <a id="platform_transition_binary-basename"></a>basename |  -   | String | optional | "" |
| <a id="platform_transition_binary-binary"></a>binary |  -   | <a href="https://bazel.build/docs/build-ref.html#labels">Label</a> | optional | None |
| <a id="platform_transition_binary-target_platform"></a>target_platform |  The target platform to transition the binary.   | <a href="https://bazel.build/docs/build-ref.html#labels">Label</a> | required |  |


<a id="platform_transition_filegroup"></a>

## platform_transition_filegroup

<pre>
platform_transition_filegroup(<a href="#platform_transition_filegroup-name">name</a>, <a href="#platform_transition_filegroup-srcs">srcs</a>, <a href="#platform_transition_filegroup-target_platform">target_platform</a>)
</pre>

Transitions the srcs to use the provided platform. The filegroup will contain artifacts for the target platform.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="platform_transition_filegroup-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/docs/build-ref.html#name">Name</a> | required |  |
| <a id="platform_transition_filegroup-srcs"></a>srcs |  The input to be transitioned to the target platform.   | <a href="https://bazel.build/docs/build-ref.html#labels">List of labels</a> | optional | [] |
| <a id="platform_transition_filegroup-target_platform"></a>target_platform |  The target platform to transition the srcs.   | <a href="https://bazel.build/docs/build-ref.html#labels">Label</a> | required |  |


