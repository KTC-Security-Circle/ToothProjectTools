add_library(runtime STATIC
  handler_context.cpp
)

target_include_directories(runtime
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(runtime
  PUBLIC
    cmd
    input
    window
    video
    structured_light
    calibration
)
