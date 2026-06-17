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
