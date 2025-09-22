# src/input/CMakeLists.txt
add_library(input
  input_handler.cpp
  bindings_default.cpp
)

target_include_directories(input
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(input
  PUBLIC
    window
  PRIVATE
    logger
    cmd_api
)

target_compile_features(input PUBLIC cxx_std_20)
