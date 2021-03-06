#!/usr/bin/env bash

emcc worker.c monolith.c -o generated/worker.js \
    --memory-init-file 0 \
    -s 'EXPORTED_FUNCTIONS=[
        "_d3cpp_set_viewport",
        "_d3cpp_set_winsize",
        "_d3cpp_set_monolith",
        ]' \
    -s 'NO_FILESYSTEM=1' \
    -s 'ALLOW_MEMORY_GROWTH=1' \
    -s 'BUILD_AS_WORKER=1' \
    -O3 \
    -std=c99 \
    -Wall
