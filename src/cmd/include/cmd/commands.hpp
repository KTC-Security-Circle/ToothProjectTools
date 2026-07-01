// src/cmd/include/cmd/commands.hpp
#pragma once
#include <filesystem>
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

/// @brief 指定cameraからframeを取得して画像ファイルに保存するcommand。
struct CmdCaptureFrame {
  /// camera_id <video::CameraId>: Capture対象のcamera識別子。
  video::CameraId camera_id{video::kInvalidCameraId};

  /// output_path <std::filesystem::path>: 保存先画像ファイルのpath。
  std::filesystem::path output_path;
};

/// @brief 左右cameraから近いタイミングでframeを取得して画像ファイルに保存するcommand。
struct CmdCaptureStereo {
  /// left_camera_id <video::CameraId>: 左側Capture対象のcamera識別子。
  video::CameraId left_camera_id{video::kInvalidCameraId};

  /// right_camera_id <video::CameraId>: 右側Capture対象のcamera識別子。
  video::CameraId right_camera_id{video::kInvalidCameraId};

  /// left_output_path <std::filesystem::path>: 左camera画像ファイルの保存先path。
  std::filesystem::path left_output_path;

  /// right_output_path <std::filesystem::path>: 右camera画像ファイルの保存先path。
  std::filesystem::path right_output_path;
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

struct CmdReconstruct {
    std::string calib_file = "calibration_stereo.yml";
    std::string scan_dir_L = "scans/L";
    std::string scan_dir_R = "scans/R";
    std::string output_ply = "reconstruction.ply";
};

using Command = std::variant<
  CmdToggleFullscreen,
  CmdMoveToMonitor,
  CmdQuit,
  CmdFocusNext,
  CmdCapturePush,
  CmdCaptureFrame,
  CmdCaptureStereo,
  CmdShowPattern,
  CmdNextPattern,
  CmdPrevPattern,
  CmdStartScan,
  CmdStopScan,
  CmdCalibrate,
  CmdCalibClear,
  CmdCalibCapture,
  CmdStereoCalibrate,
  CmdReconstruct
>;

} // namespace cmd
