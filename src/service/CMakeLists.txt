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
  PRIVATE
    cv_lib
    logger
)
