#!/usr/bin/env bash

set -e
cd "$(dirname "$0")"

cat > program.in
./build/src/mocker ./program.in
# rm ./program.in