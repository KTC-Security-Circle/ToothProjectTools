add_library(headless_dispatch STATIC
  headless_dispatcher.cpp
  headless_command_mapper.cpp
)

target_include_directories(headless_dispatch
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/control/include
)

target_link_libraries(headless_dispatch
  PUBLIC
    cmd_commands
    capture
    common
  PRIVATE
    handler
    runtime
    camera_service
    window_service
    projector_service
    scan_service
    scan_dataset_service
    logger
)

target_compile_features(headless_dispatch PUBLIC cxx_std_20)
