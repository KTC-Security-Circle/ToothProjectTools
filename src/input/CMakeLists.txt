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
    video
    window
    cmd
  PRIVATE
    logger
)

target_compile_features(input PUBLIC cxx_std_20)
