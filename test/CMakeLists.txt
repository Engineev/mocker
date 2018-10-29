set(HEADERS
  testcase.h

  test_lexer.h
  )
set(SOURCE main.cpp)

include_directories(${CMAKE_SOURCE_DIR}/src/)

add_executable(${PROJECT_NAME}_test ${SOURCE} ${HEADERS})
target_link_libraries(${PROJECT_NAME}_test ${PROJECT_NAME}_lib)

# config
add_definitions(-DCMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
