add_library(command_result_mapper STATIC
    capture_command_result_mapper.cpp
    calibration_command_result_mapper.cpp
    stereo_calibration_command_result_mapper.cpp
)

target_include_directories(command_result_mapper
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(command_result_mapper
  PUBLIC
    common
    capture
    service
)

target_compile_features(command_result_mapper PUBLIC cxx_std_20)
