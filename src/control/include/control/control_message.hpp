#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace control {

using ControlValue = std::variant<std::string, std::int64_t, bool>;
using ControlFields = std::vector<std::pair<std::string, ControlValue>>;

struct ControlMessage {
  /// id <std::optional<std::string>>: JSON Lines request/responseを対応付けるrequest id。
  std::optional<std::string> id;

  /// cmd <std::optional<std::string>>: 実行するsidecar command名。
  std::optional<std::string> cmd;

  /// role <std::optional<std::string>>: 単眼camera commandで使用するcamera role名。
  std::optional<std::string> role;

  /// output <std::optional<std::string>>: 単眼capture画像の保存先path。
  std::optional<std::string> output;

  /// left_role <std::optional<std::string>>: stereo captureで左画像に使用するcamera role名。
  std::optional<std::string> left_role;

  /// right_role <std::optional<std::string>>: stereo captureで右画像に使用するcamera role名。
  std::optional<std::string> right_role;

  /// left_output <std::optional<std::string>>: stereo capture左画像の保存先path。
  std::optional<std::string> left_output;

  /// right_output <std::optional<std::string>>: stereo capture右画像の保存先path。
  std::optional<std::string> right_output;

  /// image_folder <std::optional<std::string>>: mono calibration用画像directory。
  std::optional<std::string> image_folder;

  /// left_dir <std::optional<std::string>>: stereo calibration左画像directory。
  std::optional<std::string> left_dir;

  /// right_dir <std::optional<std::string>>: stereo calibration右画像directory。
  std::optional<std::string> right_dir;

  /// output_file <std::optional<std::string>>: calibration結果の保存先file path。
  std::optional<std::string> output_file;

  /// left_calibration_file <std::optional<std::string>>: left mono calibration file。
  std::optional<std::string> left_calibration_file;

  /// right_calibration_file <std::optional<std::string>>: right mono calibration file。
  std::optional<std::string> right_calibration_file;

  /// metadata_file <std::optional<std::string>>: scan dataset metadata file。
  std::optional<std::string> metadata_file;

  /// apply_to_camera <std::optional<bool>>: calibration結果をopen済みcameraへ反映するか。
  std::optional<bool> apply_to_camera;

  /// scan_id <std::optional<std::string>>: scan session識別子。
  std::optional<std::string> scan_id;

  /// output_dir <std::optional<std::string>>: scan dataset保存先directory。
  std::optional<std::string> output_dir;

  /// input_dir <std::optional<std::string>>: scan dataset input directory。
  std::optional<std::string> input_dir;

  /// allow_partial <std::optional<bool>>: partial datasetをvalid扱いするか。
  std::optional<bool> allow_partial;

  /// settle_ms <std::optional<int>>: pattern表示後captureまでの待機時間ms。
  std::optional<int> settle_ms;
  std::optional<std::string> sync_source;
  std::optional<int> sync_timeout_ms;
  std::optional<int> sync_guard_ms;
  std::optional<int> sync_stable_frames;
  std::optional<int> roi_x;
  std::optional<int> roi_y;
  std::optional<int> roi_width;
  std::optional<int> roi_height;
  std::optional<int> roi_black_threshold;
  std::optional<int> roi_white_threshold;

  /// threshold <std::optional<int>>: GrayCode decode threshold。
  std::optional<int> threshold;

  /// projector_width <std::optional<int>>: metadataなしdecode用projector幅。
  std::optional<int> projector_width;

  /// projector_height <std::optional<int>>: metadataなしdecode用projector高さ。
  std::optional<int> projector_height;

  /// pattern_count <std::optional<int>>: metadataなしdecode用pattern数。
  std::optional<int> pattern_count;

  /// camera_id <std::optional<int>>: open_cameraで使用するcamera識別子。
  std::optional<int> camera_id;

  /// window_role <std::optional<std::string>>: open/close対象のwindow role名。
  std::optional<std::string> window_role;

  /// title <std::optional<std::string>>: 作成するwindowのtitle。
  std::optional<std::string> title;

  /// width <std::optional<int>>: 作成するwindowの横幅。
  std::optional<int> width;

  /// height <std::optional<int>>: 作成するwindowの縦幅。
  std::optional<int> height;

  /// monitor_index <std::optional<int>>: 表示先monitor index。
  std::optional<int> monitor_index;

  /// x <std::optional<int>>: surface内またはdesktop上のX座標。
  std::optional<int> x;

  /// y <std::optional<int>>: surface内またはdesktop上のY座標。
  std::optional<int> y;

  /// placement <std::optional<std::string>>: "center" または "custom"。
  std::optional<std::string> placement;

  /// fullscreen <std::optional<bool>>: fullscreenでwindowを開くか。
  std::optional<bool> fullscreen;

  /// projector_role <std::optional<std::string>>: 操作対象projector role名。
  std::optional<std::string> projector_role;

  /// index <std::optional<int>>: 表示するpattern index。
  std::optional<int> index;

  std::optional<std::string> decode_dir;
  std::optional<std::string> calibration_file;
  std::optional<double> max_epipolar_error_px;
  std::optional<double> min_depth_mm;
  std::optional<double> max_depth_mm;
  std::optional<bool> overwrite;
  std::optional<std::string> observations_dir;
  std::optional<std::string> camera_calibration_file;
  std::optional<int> board_corners_x;
  std::optional<int> board_corners_y;
  std::optional<double> square_size_mm;
  std::optional<double> max_mean_displacement_px;
  std::optional<double> max_corner_displacement_px;
};

struct ControlError {
  /// code <std::string>: sidecar responseへ返すerror code。
  std::string code;

  /// message <std::string>: sidecar responseへ返すerror message。
  std::string message;
};

struct ControlResponse {
  /// id <std::optional<std::string>>: JSON Lines request/responseを対応付けるrequest id。
  std::optional<std::string> id;

  /// ok <bool>: command実行が成功したか。
  bool ok{false};

  /// fields <ControlFields>: responseへ追加する成功時field。
  ControlFields fields;

  /// error <std::optional<ControlError>>: command失敗時のerror情報。
  std::optional<ControlError> error;

  /// @brief 成功responseを作成する。
  ///
  /// Args:
  ///   id <std::optional<std::string>>: JSON Lines responseに付与するrequest id。
  ///   fields <ControlFields>: responseへ追加する成功時field。
  ///
  /// Return:
  ///   <ControlResponse>: ok=true のresponse。
  static ControlResponse success(
      std::optional<std::string> id,
      ControlFields fields = {});

  /// @brief 失敗responseを作成する。
  ///
  /// Args:
  ///   id <std::optional<std::string>>: JSON Lines responseに付与するrequest id。
  ///   code <std::string>: responseへ返すerror code。
  ///   message <std::string>: responseへ返すerror message。
  ///
  /// Return:
  ///   <ControlResponse>: ok=false のresponse。
  static ControlResponse failure(
      std::optional<std::string> id,
      std::string code,
      std::string message);
};

struct ControlEvent {
  /// event <std::string>: sidecar event名。
  std::string event;

  /// fields <ControlFields>: eventへ追加するfield。
  ControlFields fields;
};

} // namespace control
