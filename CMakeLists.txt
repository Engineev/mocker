cmake_minimum_required(VERSION 3.8)
project(mocker)

option(ONLINE_JUDGE_SUPPORT "" OFF)

add_subdirectory(support)
add_subdirectory(ir)
add_subdirectory(nasm)
add_subdirectory(compiler)
add_subdirectory(ir-interpreter)
