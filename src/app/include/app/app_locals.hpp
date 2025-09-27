#pragma once
// =============================================================================
//  app_locals.hpp
//  このファイルは app 実装(.cpp)間で共有する小さなヘルパのみを置く。
//  公開ヘッダ(app.hpp)に影響を与えないための分離。
// =============================================================================

#include <opencv2/highgui.hpp>
#include "window/window.hpp"

// -----------------------------------------------------------------------------
// @brief ウィンドウの存在・可視チェック
// HighGUI の特性上、イベントポンプ（waitKey/pollKey）が回っている間のみ
// プロパティが正しく反映される点に注意する。
// -----------------------------------------------------------------------------
static inline bool existsAndVisible(const win::Window& window_ref) {
  // WND_PROP_VISIBLE > 0 なら「存在・可視」
  return cv::getWindowProperty(
           window_ref.name().c_str(),
           cv::WND_PROP_VISIBLE
         ) > 0;
}
