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

/// @brief camera deviceをopenし、runtime上のcamera roleへbindするcommand。
struct CmdOpenCamera {
  /// camera_id <video::CameraId>: OpenCVへ渡すcamera device index。
  video::CameraId camera_id{video::kInvalidCameraId};

  /// role <std::string>: sidecar / runtime内でcameraを参照するrole名。
  std::string role;
};

/// @brief runtime上のcamera roleに紐づくcameraをcloseするcommand。
struct CmdCloseCamera {
  /// role <std::string>: close対象のcamera role名。
  std::string role;
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
  /// target_camera_id <video::CameraId>: mono calibration結果を適用するcamera識別子。
  video::CameraId target_camera_id;

  /// image_folder <std::string>: mono calibration画像が入っているdirectory path。
  std::string image_folder;

  /// output_file <std::string>: mono calibration結果の保存先file path。
  std::string output_file;

  /// role <std::string>: sidecar response/eventへ返すcamera role名。
  std::string role;
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
    /// left_cam_id <video::CameraId>: 左camera識別子。
    video::CameraId left_cam_id;

    /// right_cam_id <video::CameraId>: 右camera識別子。
    video::CameraId right_cam_id;

    /// left_dir <std::string>: stereo calibration左画像directory。
    std::string left_dir;

    /// right_dir <std::string>: stereo calibration右画像directory。
    std::string right_dir;

    /// output_file <std::string>: stereo calibration結果の保存先file path。
    std::string output_file = "calibration_result.yml";

    /// left_role <std::string>: sidecar response/eventへ返す左camera role名。
    std::string left_role;

    /// right_role <std::string>: sidecar response/eventへ返す右camera role名。
    std::string right_role;
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
  CmdOpenCamera,
  CmdCloseCamera,
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
