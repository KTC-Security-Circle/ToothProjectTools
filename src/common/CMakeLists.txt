add_library(common INTERFACE)

target_include_directories(common
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(common INTERFACE cxx_std_20)
