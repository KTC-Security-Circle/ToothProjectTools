# src/logger/CMakeLists.txt
add_library(logger
  logger_setup.cpp
)

target_include_directories(logger
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# ここで root の“配線ターゲット”を PUBLIC で受け取る
target_link_libraries(logger
  PUBLIC
    spd_lib
)

target_compile_features(logger PUBLIC cxx_std_20)
