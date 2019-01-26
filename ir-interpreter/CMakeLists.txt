add_executable(ir-interpreter
  main.cpp
  interpreter.h interpreter.cpp
  )
target_include_directories(ir-interpreter PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(ir-interpreter
  PRIVATE ${PROJECT_NAME}-ir ${PROJECT_NAME}-support
)
target_compile_features(ir-interpreter PRIVATE cxx_std_14)