// src/cmd/include/cmd/commands.hpp
#pragma once
#include <variant>
#include <optional>
#include <string>
#include "video/video_types.hpp"

namespace cmd {

struct CmdToggleFullscreen { };
struct CmdMoveToMonitor   { int index; };
struct CmdQuit            { };
struct CmdFocusNext       { };

// プッシュ撮影のスコープ種別
enum class CaptureScope {
  FocusedOnly,  // フォーカス中の window_id のみ撮影
  CameraGroup   // 同じ camera_id に紐づく全 window_id を撮影
};

// プッシュ撮影コマンド
struct CmdCapturePush {
  CaptureScope           scope{CaptureScope::FocusedOnly};
  std::optional<int>     camera_id{};  // 省略時はフォーカス中ウィンドウの camera_id を使用
  std::string            tag{};        // 任意ラベル（保存名等に使う想定）
};

struct CmdShowPattern { 
  int index; 
};

struct CmdNextPattern {}; 
struct CmdPrevPattern {};

struct CmdStartScan { 
  int interval_ms = 500; // パターン切り替え後の待機時間
};

// スキャン強制中断
struct CmdStopScan {};

struct CmdCalibrate {
  video::CameraId target_camera_id; // 適用先のカメラ
  std::string     image_folder;     // 画像が入っているフォルダパス
};

// キャリブレーション用フォルダのクリア
struct CmdCalibClear {
    std::string target_directory;
};

// キャリブレーション画像の撮影と保存
struct CmdCalibCapture {
    video::CameraId camera_id;
    std::string     target_directory;
    std::string     prefix = ""; // ファイル名プレフィックス (任意)
};

struct CmdStereoCalibrate {
    video::CameraId left_cam_id;
    video::CameraId right_cam_id;
    std::string left_dir;
    std::string right_dir;
    std::string output_file = "calibration_result.yml"; // 結果保存先
};

using Command = std::variant<
  CmdToggleFullscreen,
  CmdMoveToMonitor,
  CmdQuit,
  CmdFocusNext,
  CmdCapturePush,
  CmdShowPattern,
  CmdNextPattern,
  CmdPrevPattern,
  CmdStartScan,
  CmdStopScan,
  CmdCalibrate,
  CmdCalibClear,
  CmdCalibCapture,
  CmdStereoCalibrate
>;

} // namespace cmd
