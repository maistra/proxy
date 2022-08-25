# Maistra: Envoy with OpenSSL

## Rationale about this repository

This repository is a fork of the [Envoy project](https://github.com/envoyproxy/envoy), and it is modified to meet certain criteria so it can be used as the base for the OpenShift Service Mesh product by Red Hat.

The main and biggest change is the replacement of BoringSSL with OpenSSL. Other changes include some modifications to allow building on s390x and powerpc platforms and changes in the build system.

## Versions

We base our versions based on the Istio project releases. Thus, our Envoy versions, or branches depend on the Istio versions we are shipping in OpenShift Service Mesh.

Rather than using raw Envoy, we (just like Istio) build and ship Envoy not as a standalone binary, but as part of a wrapper project, Proxy.

This is the relationship between the projects: `Istio → Proxy → Envoy`. Istio and Proxy versions are the same. Example:
> Istio 1.12 comes with Proxy 1.12 which comes with Envoy 1.20.

Maistra is versioned differently, but the relationship is the same:
> Maistra 2.2 comes with Istio 1.12 which comes with Proxy 1.12 which comes with Envoy 1.20.

This means, for instance, that a branch in this repo named `maistra-2.2` is a fork of the branch `release/v1.20` of the Envoy repository.

## Build

We use a docker builder image to build and run the tests on this repository. This image contains all necessary tools to build and run the tests, like, bazel, clang, golang, etc. See <https://github.com/maistra/test-infra/tree/main/docker/> for more information on this builder image.

In order to run a full build & tests you can use the script [run-ci.sh](./run-ci.sh) like this:

```sh
$ cd /path/to/envoy # Directory where you cloned this repo

$ docker run --rm -it \
        -v $(pwd):/work \
        -u $(id -u):$(id -g) \
        --entrypoint bash \
        quay.io/maistra-dev/maistra-builder:2.2 # Make sure to use the appropriate tag, matching the branch you are working on

[user@3883abd15af2 work] ./maistra/run-ci.sh  # This runs inside the container shell
```

Inspect the [run-ci.sh script](./run-ci.sh) to see what it does and feel free to tweak it locally while doing your development builds. For instance, you can change or remove the bazel command lines that limit the number of concurrent jobs, if your machine can handle more that what's defined in that file.

An useful hint is to make use of a build cache for bazel. The [run-ci.sh script](./run-ci.sh) already supports it, but you need to perform 2 steps before invoking it:

- Create a directory in your machine that will store the cache artifacts, for example `/path/to/bazel-cache`.
- Set an ENV variable in the builder container pointing to that directory. Add this to the `docker run` command above: `-e BAZEL_DISK_CACHE=/path/to/bazel-cache`.

The [run-ci.sh script](./run-ci.sh) is the one that runs on our CI (more on this later), it builds Envoy and runs all the tests. This might take a while. Again, feel free to tweak it locally while doing your development builds. You can also totally ignore this script and run the bazel commands by hand, inside the container shell.

### Build Flags

Note that we changed a bunch of compilation flags, when comparing to upstream. Most (but not all) of these changes are on the [bazelrc](./bazelrc) file, which is included from the [main .bazelrc](../.bazelrc) file. Again, feel free to tweak these files during local development.

## Running tests


### Testing changes 

To test run Envoy's test suite locally one can run the same script that gets used in CI:

```sh
maistra/run-ci.sh
```

This will build Envoy and check that all tests pass. When this succeeds, generally CI tests will pass as well.

### Executing tests with sanitizers enabled.

Our build flows support executing Envoy's test suite with [sanitizers](https://github.com/google/sanitizers) enabled.
This allows one to test for memory leaks, undefined behaviour, data races, and so on.
The easiest way to get started is to execute the examples below in the docker image pointed out above.

```sh
# Test for undefined behaviour / leaks
maistra/test-with-asan.sh //test/common/common:base64_test
 ```

```sh
# Test for potential data races and deadlocks
maistra/test-with-tsan.sh //test/common/common:base64_test
```

```sh
# Test for uninitialized memory access
maistra/test-with-msan.sh //test/common/common:base64_test
```

## New versions

The next version is chosen based on the version the Istio project will use. For example, if Maistra 2.3 is going to ship Istio 1.14, then, we are going to use whatever version of Envoy Istio 1.14 uses (which is 1.22). That will be our `maistra-2.3` branch.

After deciding what version we are going to work on next, we start the work of porting our changes on top of the new base version - a.k.a the "rebase process".

### New builder image

We need to create a new builder image, that is capable of building the chosen Envoy version. To do that, go to the [test-infra repository](https://github.com/maistra/test-infra/tree/main/docker/), and create a new `Dockerfile` file, naming it after the new branch name. For example, if we are working on the new `maistra-2.3` branch, the filename will be `maistra-builder_2.3.Dockerfile`. We usually copy the previous file (e.g. `2.2`) and rename it. Then we look at upstream and figure out what changes should be necessary to make, for example, bumping the bazel or compiler versions, etc.

Play with this new image locally, making adjustments to the Dockerfile; build the image; run the Envoy build and tests using it; repeat until everything works. Then create a pull request on [test-infra repository](https://github.com/maistra/test-infra/) with the new Dockerfile.

---
**NOTE**

We should go through this process early in the new branch development, compiling upstream code and running upstream tests. We should not wait until the rebase process is completed to start working on the builder image. This is to make sure that our image is able to build upstream without errors. By doing this we guarantee that any error that shows up after the rebase process is done, it is our [rebase] fault.

---

### Creating the new branch and CI (Prow) jobs

We need to create jobs for the new branch in our CI (prow) system. We can split this into two tasks:

1. Create the new branch on GitHub. In our example, we are going to create the `maistra-2.3` branch that initially will be a copy of the Envoy `release/v1.22` branch.
2. Go to [test-infra repository](https://github.com/maistra/test-infra/tree/main/prow/), create new [pre and postsubmit] jobs for the `maistra-2.3` branch. Open a pull request with your changes.

After the test-infra PR above is merged, you can create a fake, trivial pull request (e.g., updating a README file) in the Envoy project, targeting the new branch. The CI job should be triggered and it must pass. If it does, close the PR. If not, figure out what's wrong (e.g., it might be the builder image is missing a build dependency), fix it (this might involve submitting a PR to the test-infra repository and wait for the new image to be pushed to quay.io after the PR is merged) and comment `/retest` in this fake PR to trigger the CI again.

### Preparing a major release

The rebase process is the process where we pick up a base branch (e.g., Envoy `release/v1.22`) and apply our changes on top of it. This is a non-trivial process as there are lots of conflicts mainly due to the way BoringSSL is integrated within the Envoy code base. This process may take several weeks. The end result should be:

- The new maistra branch (e.g., `maistra-2.3`) is created and contains the code from the desired upstream branch (e.g. Envoy `release/v1.22`) plus our changes
- Code should build and unit and integration tests must pass on our CI platform (Prow) using our builder image

It's acceptable to disable some tests, or tweak some compiler flags (e.g., disabling some `-Werror=`) in this stage, in order to keep things moving on. We should document everything that we did in order to get the build done. For instance, by adding `FIXME's` in the code and linking them to an issue in our [Jira tracker](https://issues.redhat.com/browse/OSSM).

Once the rebase is done locally, it's time to open a pull request and see how it behaves in CI. At this point we are already sure that the CI is able to run upstream code and tests successfully (see the previous topics). This means that any error caught by CI should be a legit error, caused by the changes in the rebase itself (this PR). Figure out what's wrong and keep pushing changes until CI is happy.

At the time this document is being written, an effort to automate this rebase process is being worked on. Once it finishes, this document must be updated to reflect the new process.
