#!/usr/bin/env bash

set -e
cd "$(dirname "$0")"

# cd builtin
# ./c2nasm.bash ./builtin.c
# cd ..

# ir
# g++ -c -O2 -std=c++14 -I./support \
#   -I./ir/include/ir ./ir/src/*.cpp
# ar rvs libir.a *.o
# rm *.o

# nasm
# g++ -c -O2 -std=c++14 -I./support -DONLINE_JUDGE_SUPPORT \
#   -I./nasm/include/nasm ./nasm/src/*.cpp
# ar rvs libnasm.a *.o
# rm *.o

# compiler
# g++ -DDISABLE_FORWARD_REFERENCE_FOR_GLOBAL_VAR \
#   -O2 -std=c++14 -I./support -I./ir/include -I./nasm/include -I./compiler \
#   ./compiler/ir_builder/*.cpp  \
#   ./compiler/optim/*.cpp ./compiler/optim/analysis/*.cpp \
#   ./compiler/codegen/*.cpp \
#   ./compiler/parse/*.cpp ./compiler/semantic/*.cpp ./compiler/main.cpp \
#   -o mocker-c \
#   libir.a libnasm.a


