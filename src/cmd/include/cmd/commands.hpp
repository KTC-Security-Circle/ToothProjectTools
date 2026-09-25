// src/cmd/include/cmd/commands.hpp
#pragma once
#include <filesystem>
#include <variant>
#include <optional>
#include <string>
#include "video/video_types.hpp"

namespace cmd {

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

/// @brief runtime上にwindowを作成するcommand。
struct CmdOpenWindow
{
    /// window_role <std::string>: runtime内でwindowを参照するrole名。
    std::string window_role;

    /// title <std::string>: window title。
    std::string title;

    /// width <int>: windowの横幅。
    int width{0};

    /// height <int>: windowの縦幅。
    int height{0};

    /// monitor_index <std::optional<int>>: 表示先monitor index。
    std::optional<int> monitor_index;

    /// fullscreen <bool>: fullscreenで開くか。
    bool fullscreen{false};
};

/// @brief runtime上のwindowをcloseするcommand。
struct CmdCloseWindow
{
    /// window_role <std::string>: close対象window role名。
    std::string window_role;
};


struct CmdListMonitors
{
};

struct CmdConfigureProjectorSurface
{
    /// projector_role <std::string>: surface設定対象projector role名。
    std::string projector_role;

    /// monitor_index <int>: 照射先monitor index。
    int monitor_index{0};

    /// width <int>: requested active pattern width。
    int width{0};

    /// height <int>: requested active pattern height。
    int height{0};

    /// x <std::optional<int>>: custom placement時のrequested X。
    std::optional<int> x;

    /// y <std::optional<int>>: custom placement時のrequested Y。
    std::optional<int> y;

    /// placement <std::string>: "center" または "custom"。
    std::string placement{"center"};
};

/// @brief projector roleをwindow roleへbindするcommand。
struct CmdOpenProjector
{
    /// projector_role <std::string>: runtime内でprojectorを参照するrole名。
    std::string projector_role;

    /// window_role <std::string>: pattern表示先window role名。
    std::string window_role;

    /// width <int>: pattern幅。
    int width{0};

    /// height <int>: pattern高さ。
    int height{0};
};

/// @brief projector bindingを解除するcommand。
struct CmdCloseProjector
{
    /// projector_role <std::string>: close対象projector role名。
    std::string projector_role;
};

/// @brief projector patternを生成するcommand。
struct CmdGeneratePatterns
{
    /// projector_role <std::string>: pattern生成対象projector role名。
    std::string projector_role;
};

/// @brief 指定indexのprojector patternを表示するcommand。
struct CmdProjectorShowPattern
{
    /// projector_role <std::string>: pattern表示対象projector role名。
    std::string projector_role;

    /// index <int>: 表示するpattern index。
    int index{0};

    /// photodiode_marker_mode: 指定時にsessionのmarker modeを切り替える。
    std::optional<std::string> photodiode_marker_mode;
};

/// @brief projector patternを次へ進めるcommand。
struct CmdProjectorNextPattern
{
    /// projector_role <std::string>: 操作対象projector role名。
    std::string projector_role;
};

/// @brief projector patternを前へ戻すcommand。
struct CmdProjectorPrevPattern
{
    /// projector_role <std::string>: 操作対象projector role名。
    std::string projector_role;
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

struct CmdStartScan
{
    /// scan_id <std::optional<std::string>>: scan session識別子。
    std::optional<std::string> scan_id;

    /// projector_role <std::string>: 使用projector role名。
    std::string projector_role;

    /// left_role <std::string>: 左camera role名。
    std::string left_role;

    /// right_role <std::string>: 右camera role名。
    std::string right_role;

    /// output_dir <std::string>: scan dataset保存先directory。
    std::string output_dir;

    std::string photodiode_device{"/dev/ttyUSB0"};
    int photodiode_baud{115200};
    int sync_timeout_ms{1000};
    int sync_guard_ms{30};
    int max_patterns{0};
};

struct CmdScanStatus
{
    /// scan_id <std::optional<std::string>>: status取得対象scan id。
    std::optional<std::string> scan_id;
};

struct CmdMeasureSyncDelay
{
    std::string projector_role;
    std::string camera_role;
    std::string photodiode_device{"/dev/ttyUSB0"};
    int photodiode_baud{115200};
    int transitions{60};
    int sync_timeout_ms{1000};
    double safety_margin_ms{5.0};
    double minimum_contrast{30.0};
    double required_ratio{0.90};
    std::filesystem::path output_csv{"data/photodiode_delay.csv"};
};

struct CmdStopScan
{
    /// scan_id <std::optional<std::string>>: 停止対象scan id。
    std::optional<std::string> scan_id;
};

struct CmdValidateScanDataset
{
    /// input_dir <std::string>: scan dataset directory。空の場合はleft_dir/right_dirを使う。
    std::string input_dir;

    /// allow_partial <bool>: partial datasetをvalid扱いするか。
    bool allow_partial{false};

    /// left_dir <std::string>: 明示指定されたleft画像directory。
    std::string left_dir;

    /// right_dir <std::string>: 明示指定されたright画像directory。
    std::string right_dir;

    /// metadata_file <std::string>: 明示指定されたmetadata.json path。
    std::string metadata_file;
};

struct CmdDecodePatterns
{
    /// input_dir <std::string>: scan dataset directory。空の場合はleft_dir/right_dirを使う。
    std::string input_dir;

    /// output_dir <std::string>: decode output directory。
    std::string output_dir;

    /// threshold <int>: GrayCode inverse pair threshold。
    int threshold{15};

    /// allow_partial <bool>: partial scan datasetをdecode対象として許可するか。
    bool allow_partial{false};

    /// left_dir <std::string>: 明示指定されたleft画像directory。
    std::string left_dir;

    /// right_dir <std::string>: 明示指定されたright画像directory。
    std::string right_dir;

    /// metadata_file <std::string>: 明示指定されたmetadata.json path。
    std::string metadata_file;

    /// projector_width <std::optional<int>>: metadataなしdecode用projector幅。
    std::optional<int> projector_width;

    /// projector_height <std::optional<int>>: metadataなしdecode用projector高さ。
    std::optional<int> projector_height;

    /// pattern_count <std::optional<int>>: metadataなしdecode用pattern数。
    std::optional<int> pattern_count;
};

struct CmdCalibrate {
  /// target_camera_id <video::CameraId>: mono calibration結果を適用するcamera識別子。
  video::CameraId target_camera_id;

  /// image_folder <std::string>: mono calibration画像が入っているdirectory path。
  std::string image_folder;

  /// output_file <std::string>: mono calibration結果の保存先file path。
  std::string output_file;

  /// role <std::string>: sidecar response/eventへ返すcamera role名。
  std::string role;

  /// apply_to_camera <bool>: trueの場合のみopen済みcameraへ結果を反映する。
  bool apply_to_camera{false};

  int board_corners_x{0};
  int board_corners_y{0};
  double square_size_mm{0.0};
};

struct CmdDetectCalibrationCorners {
  video::CameraId camera_id{video::kInvalidCameraId};
  std::string role;
  std::filesystem::path output_path;
  int board_corners_x{0};
  int board_corners_y{0};
  double square_size_mm{0.0};
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

    /// left_calibration_file <std::string>: left mono calibration yml。
    std::string left_calibration_file;

    /// right_calibration_file <std::string>: right mono calibration yml。
    std::string right_calibration_file;

    /// apply_to_camera <bool>: trueの場合のみopen済みcameraへ結果を反映する。
    bool apply_to_camera{false};
};

struct ReconstructionGeometryConfig { double max_epipolar_error_px{2.0}; std::optional<double> min_depth_mm; std::optional<double> max_depth_mm; };
struct CmdValidateReconstruction { std::filesystem::path decode_dir; std::filesystem::path calibration_file; ReconstructionGeometryConfig config; };
struct CmdReconstructPointCloud { std::filesystem::path decode_dir; std::filesystem::path calibration_file; std::filesystem::path output_file; ReconstructionGeometryConfig config; bool overwrite{false}; };

struct CmdCameraProjectorCalibrate
{
  std::filesystem::path observations_dir;
  std::filesystem::path camera_calibration_file;
  std::filesystem::path output_file;
  int board_corners_x{0};
  int board_corners_y{0};
  double square_size_mm{0.0};
  double max_mean_displacement_px{0.0};
  double max_corner_displacement_px{0.0};
  bool overwrite{false};
};

using Command = std::variant<
  CmdOpenCamera,
  CmdCloseCamera,
  CmdOpenWindow,
  CmdCloseWindow,
  CmdListMonitors,
  CmdConfigureProjectorSurface,
  CmdOpenProjector,
  CmdCloseProjector,
  CmdGeneratePatterns,
  CmdProjectorShowPattern,
  CmdProjectorNextPattern,
  CmdProjectorPrevPattern,
  CmdCaptureFrame,
  CmdCaptureStereo,
  CmdStartScan,
  CmdMeasureSyncDelay,
  CmdScanStatus,
  CmdStopScan,
  CmdValidateScanDataset,
  CmdDecodePatterns,
  CmdCalibrate,
  CmdCalibCapture,
  CmdDetectCalibrationCorners,
  CmdStereoCalibrate,
  CmdValidateReconstruction,
  CmdReconstructPointCloud,
  CmdCameraProjectorCalibrate
>;

} // namespace cmd
