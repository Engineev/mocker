add_library(${PROJECT_NAME}-support INTERFACE)
target_include_directories(${PROJECT_NAME}-support
  INTERFACE ${CMAKE_CURRENT_LIST_DIR}
)
target_compile_features(${PROJECT_NAME}-support INTERFACE cxx_std_14)
target_sources(${PROJECT_NAME}-support
  INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/optional.h
    ${CMAKE_CURRENT_LIST_DIR}/small_map.h
)
