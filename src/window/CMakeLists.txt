add_library(window STATIC
  window.cpp
)

target_include_directories(window
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# OpenCV 依存は Usage Requirements として公開
target_link_libraries(window
  PUBLIC
    project_wiring
)

target_compile_features(window PUBLIC cxx_std_20)
