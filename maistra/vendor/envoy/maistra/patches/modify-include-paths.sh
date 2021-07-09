#!/bin/bash

set -e
set -o pipefail
set -xi

rm -rf envoy
mv include/envoy ./
find ./envoy ./test ./source ./tools -name BUILD -print0 | xargs -0 sed -i 's/"\/\/include\//"\/\//g'
find ./source -name BUILD -print0 | xargs -0 sed -i 's/"@envoy\/\/include\//"@envoy\/\//g'
for dir in common docs exe extensions server; do
  find ./envoy ./source ./test ./tools \( -name \*.h -o -name \*.cc -o -name \*.j2 \) -print0 | xargs -0 sed -i "/Common.pb.h/!s/#include \"$dir\//#include \"source\/$dir\//g"
done
