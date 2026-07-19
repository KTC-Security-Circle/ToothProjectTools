add_library(command_result_mapper STATIC
    camera_command_result_mapper.cpp
    capture_command_result_mapper.cpp
    calibration_command_result_mapper.cpp
    stereo_calibration_command_result_mapper.cpp
    window_command_result_mapper.cpp
    projector_command_result_mapper.cpp
    scan_command_result_mapper.cpp
    scan_dataset_command_result_mapper.cpp
    decode_command_result_mapper.cpp
    reconstruction_command_result_mapper.cpp
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
    projector_service
    scan_service
    scan_dataset_service
    decode_service
    reconstruction
)

target_compile_features(command_result_mapper PUBLIC cxx_std_20)
