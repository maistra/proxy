### Utility to update dependencies on the maistra/proxy repository.

The script `update-deps.sh` will download all dependencies needed to build proxy
and will put them under `maistra/vendor` directory. It will also generate the file `maistra/bazelrc-vendor`
containing all Bazel instructions to use those downloaded dependencies. This file is already included by `.bazelrc`.

This allows an offline build of proxy.

The recommended way of using this script is through containers. Build the image in this directory:

```
$ docker build -t update-deps .
```

Then, `cd` into the top level `maistra/proxy` repository and invoke the container:

```
$ cd <path/to/maistra/proxy/directory>
$ docker run --rm -it -v $(pwd):/work -u $(id -u):$(id -g) update-deps
```

When it finishes, run `git status` and inspect the changed files. Make any necessary change and commit them.
