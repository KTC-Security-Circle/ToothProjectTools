#include "command_result_mapper/stereo_calibration_command_result_mapper.hpp"

#include <iomanip>
#include <sstream>

namespace command_result_mapper::stereo_calibration
{
namespace
{

/// @brief RMS値をresponse用文字列へ変換する。
///
/// Args:
///   rms <double>: stereo calibration RMS error。
///
/// Return:
///   <std::string>: response valuesへ設定するRMS文字列。
std::string formatRms(double rms)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(6) << rms;
    return out.str();
}

} // namespace

common::CommandResult toCommandResult(
    const std::string& left_role,
    const std::string& right_role,
    const cmd::CmdStereoCalibrate& command,
    const calib::StereoCalibrationResult& result)
{
    if (!result.ok)
    {
        const auto code = result.error ? result.error->code : std::string{"stereo_calibration_failed"};
        const auto message = result.error ? result.error->message : std::string{"failed to run stereo calibration"};
        return common::failure(code, message);
    }

    return common::success({
        {"left_role", left_role},
        {"right_role", right_role},
        {"left_dir", command.left_dir},
        {"right_dir", command.right_dir},
        {"output_file", result.output_file.string()},
        {"rms", formatRms(result.rms)},
    });
}

} // namespace command_result_mapper::stereo_calibration
