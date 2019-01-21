set(HEADERS
  ast/ast_node.h
  ast/visitor.h
  ast/fwd.h
  ast/helper.h

  common/defs.h
  common/position.h
  common/error.h

  ir_builder/builder.h
  ir_builder/builder_context.h

  parse/lexer.h
  parse/parser.h
  parse/token.h

  semantic/error.h
  semantic/semantic_checker.h
  semantic/semantic_context.h
  semantic/sym_tbl.h

  compiler.h
)
set(SOURCES
  ir_builder/builder.cpp
  ir_builder/builder_context.cpp

  parse/lexer.cpp
  parse/parser.cpp
  parse/token.cpp

  semantic/semantic_checker.cpp
  semantic/sym_tbl.cpp

  compiler.cpp
  main.cpp
)

add_executable(${PROJECT_NAME}-c ${HEADERS} ${SOURCES})
target_include_directories(${PROJECT_NAME}-c
  PRIVATE ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(${PROJECT_NAME}-c
  PRIVATE ${PROJECT_NAME}-ir ${PROJECT_NAME}-support
)
target_compile_features(${PROJECT_NAME}-c
  PRIVATE cxx_std_14
)
target_compile_definitions(${PROJECT_NAME}-c
  PRIVATE
    CMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    DISABLE_FORWARD_REFERENCE_FOR_GLOBAL_VAR
)
if (UNIX)
  target_compile_options(${PROJECT_NAME}-c PRIVATE -o2 -Wall)
endif ()
