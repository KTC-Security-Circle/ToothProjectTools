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

  /// camera_id <std::optional<int>>: open_cameraで使用するcamera識別子。
  std::optional<int> camera_id;
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
