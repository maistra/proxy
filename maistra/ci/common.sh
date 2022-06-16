OUTPUT_TO_IGNORE="\
linux-sandbox-pid1.cc|\
process-tools.cc|\
linux-sandbox.cc|\
rules_foreign_cc: Cleaning temp directories|\
proto but not used|\
INFO: From\
"

ARCH=$(uname -p)
if [ "${ARCH}" = "ppc64le" ]; then
  ARCH="ppc"
fi
export ARCH

COMMON_FLAGS="\
  --config=release \
  --config=${ARCH} \
  --local_resources 12288,4.0,1.0 \
  --jobs=4 \
  --color=no \
"

if [ -n "${BAZEL_REMOTE_CACHE}" ]; then
  COMMON_FLAGS+=" --remote_cache=${BAZEL_REMOTE_CACHE} "
elif [ -n "${BAZEL_DISK_CACHE}" ]; then
  COMMON_FLAGS+=" --disk_cache=${BAZEL_DISK_CACHE} "
fi
