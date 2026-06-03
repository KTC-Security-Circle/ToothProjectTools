add_library(runtime INTERFACE)

target_include_directories(runtime
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(runtime
  INTERFACE
    cmd
    input
    window
    video
    structured_light
    calibration
)
