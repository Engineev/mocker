set(HEADERS
  ast/ast_node.h
  ast/visitor.h
  ast/fwd.h
  ast/helper.h

  codegen/helper.h
  codegen/liveness.h
  codegen/naive_instruction_selection.h
  codegen/naive_register_allocation.h
  codegen/nasm_cfg.h
  codegen/register_allocation.h

  common/defs.h
  common/error.h
  common/position.h

  ir_builder/build.h
  ir_builder/builder.h
  ir_builder/builder_context.h
  ir_builder/preprocessor.h

  optim/analysis/func_global_var.h
  optim/constant_propagation.h
  optim/copy_propagation.h
  optim/dead_code_elimination.h
  optim/function_inline.h
  optim/helper.h
  optim/module_simplification.h
  optim/opt_pass.h
  optim/optimizer.h
  optim/promote_global_variables.h
  optim/simplify_cfg.h
  optim/ssa.h
  optim/value_numbering.h

  parse/lexer.h
  parse/parser.h
  parse/token.h

  semantic/error.h
  semantic/semantic_checker.h
  semantic/semantic_context.h
  semantic/sym_tbl.h
)
set(SOURCES
  codegen/liveness.cpp
  codegen/naive_instruction_selection.cpp
  codegen/naive_register_allocation.cpp
  codegen/nasm_cfg.cpp
  codegen/register_allocation.cpp

  ir_builder/builder.cpp
  ir_builder/builder_context.cpp
  ir_builder/preprocessor.cpp

  optim/analysis/func_global_var.cpp
  optim/constant_propagation.cpp
  optim/copy_propagation.cpp
  optim/dead_code_elimination.cpp
  optim/function_inline.cpp
  optim/helper.cpp
  optim/module_simplification.cpp
  optim/promote_global_variables.cpp
  optim/simplify_cfg.cpp
  optim/ssa.cpp
  optim/value_numbering.cpp

  parse/lexer.cpp
  parse/parser.cpp
  parse/token.cpp

  semantic/semantic_checker.cpp
  semantic/sym_tbl.cpp

  main.cpp
)

add_executable(${PROJECT_NAME}-c ${HEADERS} ${SOURCES})
target_include_directories(${PROJECT_NAME}-c
  PRIVATE ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(${PROJECT_NAME}-c
  PRIVATE ${PROJECT_NAME}-ir ${PROJECT_NAME}-support ${PROJECT_NAME}-nasm
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
