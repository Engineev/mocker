add_library(${PROJECT_NAME}-nasm
  ${CMAKE_CURRENT_LIST_DIR}/include/nasm/addr.h
  ${CMAKE_CURRENT_LIST_DIR}/include/nasm/helper.h
  ${CMAKE_CURRENT_LIST_DIR}/include/nasm/inst.h
  ${CMAKE_CURRENT_LIST_DIR}/include/nasm/module.h
  ${CMAKE_CURRENT_LIST_DIR}/include/nasm/printer.h
  ${CMAKE_CURRENT_LIST_DIR}/include/nasm/stats.h

  ${CMAKE_CURRENT_LIST_DIR}/src/helper.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/module.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/printer.cpp
  src/helper.cpp)
target_include_directories(${PROJECT_NAME}-nasm
  INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include/nasm
)
if (ONLINE_JUDGE_SUPPORT)
  target_compile_definitions(${PROJECT_NAME}-nasm
    PRIVATE
      ONLINE_JUDGE_SUPPORT
  )
endif()
target_link_libraries(${PROJECT_NAME}-nasm PRIVATE ${PROJECT_NAME}-support)
target_compile_features(${PROJECT_NAME}-nasm PUBLIC cxx_std_14)
