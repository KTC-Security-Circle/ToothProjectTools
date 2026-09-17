add_library(sidecar_service STATIC sidecar_service.cpp)
target_include_directories(sidecar_service PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(sidecar_service
  PUBLIC camera_service window_service monitor_service projector_service scan_service
         scan_dataset_service decode_service capture calibration reconstruction mjpeg_stream video
  PRIVATE cv_lib logger
)
target_compile_features(sidecar_service PUBLIC cxx_std_20)

add_library(sidecar_serve STATIC
  serve_app.cpp
)

target_include_directories(sidecar_serve
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(sidecar_serve
  PUBLIC
    sidecar_control
    sidecar_service
    mjpeg_stream
  PRIVATE
    logger
)

target_compile_features(sidecar_serve PUBLIC cxx_std_20)

add_executable(tooth_backend
  main.cpp
)

set_target_properties(tooth_backend PROPERTIES OUTPUT_NAME "tooth-backend")

target_link_libraries(tooth_backend PRIVATE sidecar_serve logger)
target_compile_features(tooth_backend PRIVATE cxx_std_20)
