add_library(cmd_commands INTERFACE)

target_include_directories(cmd_commands
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cmd_commands
  INTERFACE
    video
    calibration
)

# ヘッダ専用ライブラリ
add_library(cmd INTERFACE)

target_link_libraries(cmd
  INTERFACE
    cmd_commands
    window
)
