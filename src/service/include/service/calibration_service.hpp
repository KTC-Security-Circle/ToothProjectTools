#pragma once

#include "cmd/commands.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace calib { class Calibrator; }
namespace video { class CameraManager; }

namespace service::calibration
{

struct MonoCalibrationError
{
    /// code <std::string>: mono calibration失敗時のerror code。
    std::string code;

    /// message <std::string>: mono calibration失敗時のerror message。
    std::string message;
};

struct MonoCalibrationResult
{
    /// ok <bool>: mono calibrationが成功したか。
    bool ok{false};

    /// rms <double>: calibration RMS error。
    double rms{0.0};

    /// output_file <std::filesystem::path>: calibration結果の保存先file path。
    std::filesystem::path output_file;

    /// error <std::optional<MonoCalibrationError>>: mono calibration失敗時のerror情報。
    std::optional<MonoCalibrationError> error;
};

/// @brief 保存済み単眼画像からmono calibrationを実行する。
///
/// Args:
///   cameras <video::CameraManager&>: calibration結果の適用先cameraを管理するmanager。
///   calibrator <calib::Calibrator*>: mono calibration計算器。nullの場合は失敗を返す。
///   command <const cmd::CmdCalibrate&>: mono calibration command。
///
/// Return:
///   <MonoCalibrationResult>: calibration成否、RMS、出力file、失敗時error。
MonoCalibrationResult calibrate(video::CameraManager& cameras, calib::Calibrator* calibrator,
                                const cmd::CmdCalibrate& command);

} // namespace service::calibration
