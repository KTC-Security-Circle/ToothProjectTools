add_library(sidecar_control STATIC
  control_input_adapter.cpp
  control_message.cpp
  json_line_reader.cpp
  json_line_writer.cpp
)

target_include_directories(sidecar_control
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(sidecar_control
  PUBLIC
    sidecar_service
  PRIVATE
    cv_lib
)

target_compile_features(sidecar_control PUBLIC cxx_std_20)
