add_library(headless_command_execution STATIC
  headless_command_executor.cpp
  headless_command_executor_camera.cpp
  headless_command_executor_projector.cpp
  headless_command_executor_scan.cpp
  headless_command_executor_calibration.cpp
  headless_command_mapper.cpp
)

target_include_directories(headless_command_execution
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/control/include
)

target_link_libraries(headless_command_execution
  PUBLIC
    cmd_commands
    capture
    common
  PRIVATE
    command_result_mapper
    video
    calibration
    camera_service
    window_service
    projector_service
    scan_service
    scan_dataset_service
    decode_service
    reconstruction
    logger
)

target_compile_features(headless_command_execution PUBLIC cxx_std_20)
