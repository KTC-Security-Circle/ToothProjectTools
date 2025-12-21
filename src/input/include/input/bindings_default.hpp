#pragma once
#include "input/input_handler.hpp"
#include "cmd/dispatch_cmd.hpp"
#include "window/window.hpp"
#include "video/camera.hpp"
#include <deque>
#include <functional>
#include <string>
#include <tuple>

// 依存関係を減らすため、CameraId等はintや前方宣言で扱うか、必要なヘッダをインクルード
#include "video/video_types.hpp" // video::CameraId

namespace input {

// キャリブレーション用の設定コンテナ
struct CalibrationBindConfig {
    video::CameraId scan_cam_left;
    video::CameraId scan_cam_right;
    // 単眼用 (Mono)
    std::string dir_mono_left;
    std::string dir_mono_right;

    // ステレオ用 (Stereo)
    std::string dir_stereo_left;
    std::string dir_stereo_right;
};

// フォーカス中のターゲットを特定するためのコールバック型
// 返り値: {対象カメラID, 保存先ディレクトリ}
using TargetResolver = std::function<std::pair<video::CameraId, std::string>()>;

void install_default_bindings(
  InputHandler& handler,
  std::deque<DispatchCmd>& cmd_que,
  win::WindowId projector_id,
  // ★追加
  const CalibrationBindConfig& calib_config,
  TargetResolver get_focused_target
);

} // namespace input