add_library(handler STATIC
    camera_command_handler.cpp
    global_command_handler.cpp
    window_command_handler.cpp
    pattern_command_handler.cpp
    scan_command_handler.cpp
    calibration_command_handler.cpp
    capture_command_handler.cpp
    stereo_calibration_command_handler.cpp
    reconstruction_command_handler.cpp
)

target_include_directories(handler
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(handler
  PUBLIC
    cmd
    common
    command_result_mapper
  PRIVATE
    runtime
    window
    video
    structured_light
    calibration
    service
    capture
    cv_lib
    logger
)
