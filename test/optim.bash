#!/usr/bin/env bash

set -e
cd "$(dirname "$0")"
cd ..

cat > program.in
./build/compiler/mocker-c ./program.in > a.asm
cat a.asm ./builtin/builtin.asm
# rm ./program.in
