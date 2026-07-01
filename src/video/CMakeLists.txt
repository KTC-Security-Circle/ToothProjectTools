# src/video/CMakeLists.txt
add_library(video
  camera.cpp
  camera_manager.cpp
)

target_include_directories(video
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(video
  PUBLIC
    cv_lib
  PRIVATE
    logger
)

target_compile_features(video PUBLIC cxx_std_20)
