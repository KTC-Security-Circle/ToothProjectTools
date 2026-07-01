add_library(mjpeg_stream STATIC
  frame_publisher.cpp
  mjpeg_server.cpp
  stream_registry.cpp
  role_name.cpp
)

target_include_directories(mjpeg_stream
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(mjpeg_stream
  PUBLIC
    video
  PRIVATE
    cv_lib
    logger
)

target_compile_features(mjpeg_stream PUBLIC cxx_std_20)
