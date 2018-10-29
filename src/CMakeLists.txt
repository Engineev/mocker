set(HEADERS
  common/defs.h
  common/position.h

  lex/lexer.h
  lex/token.h

  compiler.h
)
set(LIB_SOURCES
  lex/lexer.cpp

  compiler.cpp
)

include_directories(${CMAKE_CURRENT_LIST_DIR})

add_library(${PROJECT_NAME}_lib ${LIB_SOURCES} ${HEADERS})
add_executable(${PROJECT_NAME} main.cpp ${HEADERS})
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}_lib)
