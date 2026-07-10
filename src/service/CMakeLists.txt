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

add_library(camera_service STATIC
  camera_service.cpp
)

target_include_directories(camera_service PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(camera_service PUBLIC video)
target_compile_features(camera_service PUBLIC cxx_std_20)

add_library(window_service STATIC
  window_result.cpp
  window_service.cpp
)

target_include_directories(window_service PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(window_service PUBLIC window PRIVATE logger)
target_compile_features(window_service PUBLIC cxx_std_20)

add_library(projector_service STATIC
  projector_result.cpp
  projector_service.cpp
)

target_include_directories(projector_service PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(projector_service PUBLIC window_service structured_light)
target_compile_features(projector_service PUBLIC cxx_std_20)

add_library(sidecar_service STATIC
  sidecar_service.cpp
)

target_include_directories(sidecar_service
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(sidecar_service
  PUBLIC
    camera_service
    window_service
    projector_service
    capture
    calibration
    mjpeg_stream
    video
  PRIVATE
    cv_lib
    logger
)

target_compile_features(sidecar_service PUBLIC cxx_std_20)
