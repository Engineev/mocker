cmake_minimum_required(VERSION 3.5)
project(mocker)

set(CMAKE_CXX_STANDARD 14)

if (UNIX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -o2 -Wall")
endif ()

add_definitions(-DCMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

add_subdirectory(src)
add_subdirectory(test)
