add_library(command_result_mapper STATIC
    camera_command_result_mapper.cpp
    capture_command_result_mapper.cpp
    calibration_command_result_mapper.cpp
    stereo_calibration_command_result_mapper.cpp
    window_command_result_mapper.cpp
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
    window_service
)

target_compile_features(command_result_mapper PUBLIC cxx_std_20)
