set(HEADERS
  ast/ast_node.h
  ast/visitor.h
  ast/fwd.h
  ast/helper.h

  codegen/helper.h
  codegen/instruction_selection.h
  codegen/liveness.h
  codegen/naive_register_allocation.h
  codegen/nasm_cfg.h
  codegen/peephole.h
  codegen/register_allocation.h
  codegen/vreg_assignment.h

  common/defs.h
  common/error.h
  common/position.h

  ir_builder/build.h
  ir_builder/builder.h
  ir_builder/builder_context.h
  ir_builder/preprocessor.h

  optim/analysis/defuse.h
  optim/analysis/dominance.h
  optim/analysis/func_attr.h
  optim/analysis/loop_info.h
  optim/codegen_prepare.h
  optim/constant_propagation.h
  optim/copy_propagation.h
  optim/dead_code_elimination.h
  optim/function_inline.h
  optim/global_const_inline.h
  optim/global_value_numbering.h
  optim/helper.h
  optim/local_value_numbering.h
  optim/loopinv.cpp
  optim/memorization.cpp
  optim/module_simplification.h
  optim/opt_pass.h
  optim/optimizer.h
  optim/promote_global_variables.h
  optim/reassociation.h
  optim/simplify_cfg.h
  optim/ssa.h

  parse/lexer.h
  parse/parser.h
  parse/token.h

  semantic/error.h
  semantic/semantic_checker.h
  semantic/semantic_context.h
  semantic/sym_tbl.h
)
set(SOURCES
  codegen/instruction_selection.cpp
  codegen/liveness.cpp
  codegen/naive_register_allocation.cpp
  codegen/nasm_cfg.cpp
  codegen/peephole.cpp
  codegen/register_allocation.cpp
  codegen/vreg_assignment.cpp

  ir_builder/build.cpp
  ir_builder/builder.cpp
  ir_builder/builder_context.cpp
  ir_builder/preprocessor.cpp

  optim/analysis/dominance.cpp
  optim/analysis/func_attr.cpp
  optim/analysis/loop_info.cpp
  optim/codegen_prepare.cpp
  optim/constant_propagation.cpp
  optim/copy_propagation.cpp
  optim/dead_code_elimination.cpp
  optim/function_inline.cpp
  optim/global_constant_inline.cpp
  optim/global_value_numbering.cpp
  optim/helper.cpp
  optim/local_value_numbering.cpp
  optim/loopinv.cpp
  optim/memorization.cpp
  optim/module_simplification.cpp
  optim/promote_global_variables.cpp
  optim/reassociation.cpp
  optim/simplify_cfg.cpp
  optim/ssa.cpp

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
