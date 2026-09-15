#pragma once

#include "cmd/commands.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace calib { class StereoCalibrator; struct StereoData; }
namespace video { class CameraManager; }

namespace calib
{

struct StereoCalibrationError
{
    /// code <std::string>: stereo calibration失敗時のerror code。
    std::string code;

    /// message <std::string>: stereo calibration失敗時のerror message。
    std::string message;
};

struct StereoCalibrationResult
{
    /// ok <bool>: stereo calibrationが成功したか。
    bool ok{false};

    /// rms <double>: stereo calibration RMS error。
    double rms{0.0};

    /// output_file <std::filesystem::path>: stereo calibration結果の保存先file path。
    std::filesystem::path output_file;

    /// error <std::optional<StereoCalibrationError>>: stereo calibration失敗時のerror情報。
    std::optional<StereoCalibrationError> error;
};

/// @brief 保存済み左右画像pairからstereo calibrationを実行する。
///
/// Args:
///   cameras <video::CameraManager&>: calibration対象cameraを管理するmanager。
///   stereo_calibrator <calib::StereoCalibrator*>: stereo calibration計算器。nullの場合は失敗を返す。
///   stereo_data <calib::StereoData&>: 成功したcalibration結果の保存先。
///   command <const cmd::CmdStereoCalibrate&>: stereo calibration command。
///
/// Return:
///   <StereoCalibrationResult>: calibration成否、RMS、出力file、失敗時error。
StereoCalibrationResult calibrate(video::CameraManager& cameras, calib::StereoCalibrator* stereo_calibrator,
                                  calib::StereoData& stereo_data, const cmd::CmdStereoCalibrate& command);

} // namespace calib
