cmake_minimum_required(VERSION 3.20)

# =============================================================================
# Project
# =============================================================================
project(structured_dev LANGUAGES CXX)

# =============================================================================
# C++ Standard / Build-Type / Tooling
# =============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 既定ビルドタイプ（未指定なら Release）
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# 警告フラグ（必要に応じて調整）
add_compile_options(-Wall -Wextra -Wpedantic)

# mold を使う（CMake>=3.29 は LINKER_TYPE、未満は -fuse-ld=mold）
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.29")
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

# =============================================================================
# Dependencies (OpenCV / spdlog via FetchContent)
# =============================================================================
# OpenCV（配布版を find_package）
find_package(
  OpenCV 4.10.0 EXACT
  REQUIRED
  CONFIG
  COMPONENTS
    core
    imgproc
    highgui
    videoio
    imgcodecs
    calib3d
    features2d
    flann
    structured_light
    phase_unwrapping
)

message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV config: ${OpenCV_CONFIG_PATH}")
message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")

# spdlog を FetchContent で取得（バージョン固定）
include(FetchContent)
set(SPDLOG_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL  OFF CACHE BOOL "" FORCE)  # 同梱の fmt を使用
FetchContent_Declare(
  spdlog
  URL https://github.com/gabime/spdlog/archive/refs/tags/v1.15.3.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(spdlog)  # spdlog::spdlog / spdlog::spdlog_header_only

# =============================================================================
# Wiring (共通依存を束ねて伝播させる INTERFACE ターゲット)
# =============================================================================
add_library(spd_lib INTERFACE)
target_link_libraries(spd_lib
  INTERFACE
    spdlog::spdlog_header_only
)

add_library(cv_lib INTERFACE)

target_include_directories(
  cv_lib
  INTERFACE
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(
  cv_lib
  INTERFACE
    ${OpenCV_LIBS}
)

# =============================================================================
# Static Analysis (cppcheck)
#  - グローバル既定として設定（ターゲット作成時に反映）
#  - _deps を確実に除外するため --suppress を併用
# =============================================================================
option(ENABLE_CPPCHECK "Run cppcheck at build time" ON)
find_program(CPPCHECK_EXE NAMES cppcheck)
if(ENABLE_CPPCHECK AND CPPCHECK_EXE)
add_custom_target(cppcheck-project
  COMMAND ${CPPCHECK_EXE}
          --enable=warning,style,performance
          --inline-suppr --quiet
          --project=${CMAKE_BINARY_DIR}/compile_commands.json
          -i ${CMAKE_BINARY_DIR}/_deps
          --suppress=*:${CMAKE_BINARY_DIR}/_deps/*
          --suppressions-list=${CMAKE_SOURCE_DIR}/cppcheck.supp
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Running cppcheck (project mode)"
)
endif()

# =============================================================================
# Subdirectories (Targets live here)
# =============================================================================
add_subdirectory(src/logger)  # ライブラリ（公開ヘッダ・依存はここで宣言）
add_subdirectory(src/common)
add_subdirectory(src/window)
add_subdirectory(src/video)
add_subdirectory(src/capture)
add_subdirectory(src/stream)
add_subdirectory(src/structured_light)
add_subdirectory(src/calibration)
add_subdirectory(src/reconstruction)
add_subdirectory(src/cmd)
add_subdirectory(src/input)
add_subdirectory(src/runtime)  # INTERFACE ターゲット（依存の伝播先）
add_subdirectory(src/service)
add_subdirectory(src/command_result_mapper)
add_subdirectory(src/handler)
add_subdirectory(src/headless)
add_subdirectory(src/control)
add_subdirectory(src/serve)
add_subdirectory(src/dispatch)  # INTERFACE ターゲット（依存の伝播先）
add_subdirectory(src/app)     # 実行ファイル（logger + project_wiring に依存）

enable_testing()
add_subdirectory(tests)
