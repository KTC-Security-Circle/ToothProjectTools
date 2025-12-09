# src/structured_light/CMakeLists.txt
add_library(structured_light
  structured_light.cpp
)

target_include_directories(structured_light
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(structured_light
  PRIVATE
    cv_lib
    logger
)

target_compile_features(structured_light PUBLIC cxx_std_20)
