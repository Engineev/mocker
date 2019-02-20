add_library(${PROJECT_NAME}-ir
  include/ir/helper.h
  include/ir/ir_inst.h
  include/ir/module.h
  include/ir/printer.h
  include/ir/stats.h

  src/helper.cpp
  src/module.cpp
  src/printer.cpp
)
target_include_directories(${PROJECT_NAME}-ir
  INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include/ir
)
target_link_libraries(${PROJECT_NAME}-ir PRIVATE ${PROJECT_NAME}-support)
target_compile_features(${PROJECT_NAME}-ir PUBLIC cxx_std_14)
