## Container image to build maistra/proxy.

## Build proxy

This image allows an offline build of `maistra/istio`.

To build the image, run:
```
$ make
```

An image named `maistra/proxy-build:maistra-1.2` has been created. Now you can build proxy within that image. Just `cd` into the top level `maistra/proxy` repository and invoke the container:

```
$ cd <path/to/maistra/proxy/directory>
$ docker run --network=none --rm -it -v $(pwd):/work -u $(id -u):$(id -g) maistra/proxy-build:maistra-1.2

# Within it, build proxy:
# bazel build //src/envoy:envoy
```

Within the container, inspect `bazel-bin/` directory for the generated binary.

## Update proxy dependencies
Run:
```
$ cd <path/to/maistra/proxy/directory>
$ docker run --rm -it -v $(pwd):/work -u $(id -u):$(id -g) maistra/proxy-build:maistra-1.2 update-deps.sh
```
This will populate the directory maistra/vendor with all dependencies needed to build proxy offline.