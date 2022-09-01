#!/bin/bash
echo "*** local_link_to_host  ***"
rm -f /work/maistra/local/bin/*
ln -s /bin/node /work/maistra/local/bin/node
ln -s /usr/bin/wasm-emscripten-finalize /work/maistra/local/bin/wasm-emscripten-finalize
ln -s /usr/bin/wasm-opt /work/maistra/local/bin/wasm-opt

