#!/usr/bin/env bash

set -e
cd "$(dirname "$0")"

cat > program.in
./mocker-c ./program.in --semantic

# rm ./program.in