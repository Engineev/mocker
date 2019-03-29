#!/usr/bin/env bash

set -e
cd "$(dirname "$0")"

cat > program.in

./build/compiler/mocker-c ./program.in  # cmake version
# ./mocker-c ./program.in

# rm ./program.in