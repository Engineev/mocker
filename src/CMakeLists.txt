set(HEADERS
  common/defs.h
  common/position.h

  lex/lexer.h
  lex/token.h

  compiler.h
)
set(SOURCES main.cpp
  lex/lexer.cpp

  compiler.cpp
)

include_directories(${CMAKE_CURRENT_LIST_DIR})

add_executable(mocker ${SOURCES} ${HEADERS})
