#!/usr/bin/env bash

set -e
cd "$(dirname "$0")"

if [[ ! -d "./build/" ]]; then
  mkdir build;
fi;
cd build
cmake ..
make

#g++ -O2 -std=c++14 -I./support -I./ir/include -I./ir/include/ir -I./compiler \
#  ./ir/src/*.cpp ./compiler/ir_builder/*.cpp ./compiler/optim/*.cpp \
#  ./compiler/parse/*.cpp ./compiler/semantic/*.cpp ./compiler/main.cpp \
#  -o mocker-c
