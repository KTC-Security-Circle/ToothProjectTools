# ヘッダ専用ライブラリ
add_library(cmd_api INTERFACE)

target_include_directories(cmd_api
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# もし target.hpp が win::Window::Id に依存するなら↓を有効化
# target_link_libraries(cmd_api INTERFACE window)
