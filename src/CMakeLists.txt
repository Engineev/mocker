set(HEADERS
  ast/ast_node.h
  ast/visitor.h
  ast/fwd.h
#  ast/printer.h

  common/defs.h
  common/position.h
  common/error.h

  parse/lexer.h
  parse/parser.h
  parse/token.h

  support/optional.h
  support/small_map.h

  compiler.h
)
set(LIB_SOURCES
#  ast/printer.cpp

  parse/lexer.cpp
  parse/parser.cpp
  parse/token.cpp

  compiler.cpp
)

include_directories(${CMAKE_CURRENT_LIST_DIR})

add_library(${PROJECT_NAME}_lib ${LIB_SOURCES} ${HEADERS})
add_executable(${PROJECT_NAME} main.cpp ${HEADERS})
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}_lib)
