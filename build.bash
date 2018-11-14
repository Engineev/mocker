#!/usr/bin/env bash

set -e
cd "$(dirname "$0")"

if [[ ! -d "./build/" ]]; then
  mkdir build;
fi;
cd build
cmake ..
make
