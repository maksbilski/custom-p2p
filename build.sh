#!/bin/bash

rm -r build

mkdir build

cd build

cmake ..

make

./p2p_resource_sync_tests
