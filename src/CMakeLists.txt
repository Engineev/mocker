# set(HEADERS
# )
set(SOURCES main.cpp
)

include_directories(${CMAKE_CURRENT_LIST_DIR})
find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

add_executable(mocker ${SOURCES})
