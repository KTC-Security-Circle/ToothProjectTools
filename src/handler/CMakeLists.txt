add_library(handler STATIC
    global_command_handler.cpp
    window_command_handler.cpp
    pattern_command_handler.cpp
    scan_command_handler.cpp
    calibration_command_handler.cpp
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
  PRIVATE
    runtime
    window
    video
    structured_light
    calibration
    cv_lib
    logger
)
