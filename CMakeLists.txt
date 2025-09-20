cmake_minimum_required(VERSION 3.20)
project(my_project LANGUAGES CXX)

# ===== 基本設定 =====
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 既定ビルドタイプ（未指定なら Release）
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# 警告（必要に応じて調整）
add_compile_options(-Wall -Wextra -Wpedantic)

# mold を使う（CMake>=3.28 は LINKER_TYPE、未満は -fuse-ld=mold）
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.29")
  # 参考: https://cmake.org/cmake/help/latest/variable/CMAKE_LINKER_TYPE.html
  set(CMAKE_LINKER_TYPE MOLD)
else()
  add_link_options(-fuse-ld=mold)
endif()

# ccache（見つかった時だけ）
find_program(CCACHE_EXECUTABLE ccache)
if(CCACHE_EXECUTABLE)
  set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_EXECUTABLE}")
  set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_EXECUTABLE}")
endif()

# ===== 依存の検出 / 取得 =====
# OpenCV（配布版を find_package）
# 参考: https://docs.opencv.org/ (find_package(OpenCV))
find_package(OpenCV REQUIRED COMPONENTS
  core imgproc highgui videoio imgcodecs calib3d features2d flann)

# （任意）contrib があれば後でリンクできるように存在チェックだけ
find_library(OPENCV_STRUCTURED_LIGHT NAMES opencv_structured_light)
find_library(OPENCV_PHASE_UNWRAPPING NAMES opencv_phase_unwrapping)
if(NOT OPENCV_STRUCTURED_LIGHT OR NOT OPENCV_PHASE_UNWRAPPING)
  message(STATUS "opencv_contrib 未検出（structured_light / phase_unwrapping）。必要なら 'libopencv-contrib-dev' を追加してください。")
endif()

find_program(CPPCHECK_EXE NAMES cppcheck)

if (CPPCHECK_EXE)
  set(_CPPCHECK_OPTS
    --enable=warning,style,performance
    --inline-suppr
    --quiet
    --language=c++
    --std=c++20
    --suppressions-list=${CMAKE_SOURCE_DIR}/cppcheck.supp
    -i ${CMAKE_BINARY_DIR}/_deps
  )

  # セミコロンで区切ると CMake がリスト化してくれるので
  # 文字列内に余計なセミコロンは書かない
  set_property(TARGET ${TARGET_NAME} PROPERTY CXX_CPPCHECK
    ${CPPCHECK_EXE} ${_CPPCHECK_OPTS})
endif()

# spdlog を FetchContent で取得（バージョン固定、テスト/例は無効化）
# 参考: https://cmake.org/cmake/help/latest/module/FetchContent.html
#       https://github.com/gabime/spdlog
include(FetchContent)
set(SPDLOG_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL  OFF CACHE BOOL "" FORCE)   # 同梱のfmtを使用
FetchContent_Declare(
  spdlog
  URL https://github.com/gabime/spdlog/archive/refs/tags/v1.15.3.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  # URL_HASH を付けるならここに（任意）
)
FetchContent_MakeAvailable(spdlog)
# → ターゲット: spdlog::spdlog / spdlog::spdlog_header_only

# spdlog のターゲットでは cppcheck を走らせない
if (TARGET spdlog)
  set_property(TARGET spdlog PROPERTY CXX_CPPCHECK "")
endif()

# ===== 実行ターゲット =====
# 変数名は紛らわしいので PROJECT_NAME と区別しやすいものに
set(TARGET_NAME structured_light_decoder)

add_executable(${TARGET_NAME}
  src/main.cpp
  src/logger/logger_setup.cpp
)

target_include_directories(${TARGET_NAME}
  PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(${TARGET_NAME} PRIVATE spdlog::spdlog ${OpenCV_LIBS})
if(OPENCV_STRUCTURED_LIGHT)
  target_link_libraries(${TARGET_NAME} PRIVATE ${OPENCV_STRUCTURED_LIGHT})
endif()
if(OPENCV_PHASE_UNWRAPPING)
  target_link_libraries(${TARGET_NAME} PRIVATE ${OPENCV_PHASE_UNWRAPPING})
endif()
