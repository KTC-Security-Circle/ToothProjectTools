#include "command_result_mapper/calibration_command_result_mapper.hpp"

#include <iomanip>
#include <sstream>

namespace command_result_mapper::calibration
{
namespace
{

/// @brief RMS値をresponse用文字列へ変換する。
///
/// Args:
///   rms <double>: calibration RMS error。
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
    const std::string& role,
    const cmd::CmdCalibrate& command,
    const calib::MonoCalibrationResult& result)
{
    if (!result.ok)
    {
        const auto code = result.error ? result.error->code : std::string{"calibration_failed"};
        const auto message = result.error ? result.error->message : std::string{"failed to run mono calibration"};
        return common::failure(code, message);
    }

    return common::success({
        {"role", role},
        {"image_folder", command.image_folder},
        {"output_file", result.output_file.string()},
        {"rms", formatRms(result.rms)},
    });
}

} // namespace command_result_mapper::calibration
