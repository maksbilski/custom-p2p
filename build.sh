#!/bin/bash

BUILD_TESTS=${1:-OFF}

mkdir -p build

cmake -B build -DBUILD_TESTS=$BUILD_TESTS

cmake --build build

if [ "$BUILD_TESTS" = "ON" ]; then
    cd build && ctest --output-on-failure
fi

