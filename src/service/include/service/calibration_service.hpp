#pragma once

#include "cmd/commands.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace runtime
{
struct CalibrationHandlerContext;
struct MonoCalibrationCalcContext;
}

namespace win
{
class Window;
}

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

void clear(runtime::CalibrationHandlerContext& ctx, win::Window& target_window, const cmd::CmdCalibClear& command);
void capture(runtime::CalibrationHandlerContext& ctx, win::Window& target_window, const cmd::CmdCalibCapture& command);

/// @brief 保存済み単眼画像からmono calibrationを実行する。
///
/// Args:
///   ctx <runtime::MonoCalibrationCalcContext&>: camera managerとcalibratorを持つ計算用context。
///   command <const cmd::CmdCalibrate&>: mono calibration command。
///
/// Return:
///   <MonoCalibrationResult>: calibration成否、RMS、出力file、失敗時error。
MonoCalibrationResult calibrate(runtime::MonoCalibrationCalcContext& ctx, const cmd::CmdCalibrate& command);

/// @brief GUI用contextから保存済み単眼画像のmono calibrationを実行する。
///
/// Args:
///   ctx <runtime::CalibrationHandlerContext&>: GUI handler用context。
///   command <const cmd::CmdCalibrate&>: mono calibration command。
///
/// Return:
///   <MonoCalibrationResult>: calibration成否、RMS、出力file、失敗時error。
MonoCalibrationResult calibrate(runtime::CalibrationHandlerContext& ctx, const cmd::CmdCalibrate& command);

} // namespace service::calibration
