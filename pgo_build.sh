#!/bin/sh

rm -rfv build
mkdir -pv build
cd build || exit 1

CXX_FLAGS="-march=native -flto -pipe"

cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-fprofile-generate ${CXX_FLAGS}"
make -j "$(nproc)"

./ge-inspector

cmake .. -DCMAKE_CXX_FLAGS="-fprofile-use ${CXX_FLAGS}"
make -j "$(nproc)"
