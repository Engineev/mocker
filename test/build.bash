#!/usr/bin/env bash

set -e
cd "$(dirname "$0")"
cd ..

#cd builtin
#./c2nasm.bash ./builtin.c
#cd ..

if [[ ! -d "./build/" ]]; then
  mkdir build;
fi;
cd build
cmake -DONLINE_JUDGE_SUPPORT=ON ..
make
