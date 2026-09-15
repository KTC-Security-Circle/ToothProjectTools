add_library(decode_service STATIC
  decode_result.cpp
  decode_service.cpp
)

target_include_directories(decode_service PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(decode_service PUBLIC scan_dataset_service cv_lib)
target_compile_features(decode_service PUBLIC cxx_std_20)
