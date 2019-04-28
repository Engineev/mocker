#!/usr/bin/env bash

set -e
cd "$(dirname "$0")"

cat > program.in
./mocker-c ./program.in > a.asm
cat a.asm ./builtin/builtin.asm

