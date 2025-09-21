add_library(window STATIC
  window.cpp
  monitor_x11.cpp
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

# --- X11 / Xrandr を検出してリンク ---
find_package(X11 REQUIRED)               # X11::X11
# Xrandr は X11 の一部として提供されることが多いが、個別に探す
find_library(XRANDR_LIB Xrandr)
if(NOT XRANDR_LIB)
  message(FATAL_ERROR "libXrandr (Xrandr) が見つかりません。Debian/Ubuntu: apt-get install libxrandr-dev")
endif()

target_link_libraries(window PRIVATE X11::X11 ${XRANDR_LIB})

target_compile_features(window PUBLIC cxx_std_20)
