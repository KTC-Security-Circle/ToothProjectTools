add_library(service STATIC
    scan_service.cpp
    calibration_service.cpp
    stereo_calibration_service.cpp
    reconstruction_service.cpp
)

target_include_directories(service
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(service
  PUBLIC
    runtime
    capture
  PRIVATE
    cv_lib
    logger
)

add_library(sidecar_service STATIC
  sidecar_service.cpp
)

target_include_directories(sidecar_service
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(sidecar_service
  PUBLIC
    mjpeg_stream
    video
  PRIVATE
    cv_lib
    logger
)

target_compile_features(sidecar_service PUBLIC cxx_std_20)
