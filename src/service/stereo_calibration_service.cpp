#include "service/stereo_calibration_service.hpp"

#include "calibration/stereo_calibrator.hpp"
#include "calibration/stereo_data.hpp"
#include "logger/logger_macros.hpp"
#include "runtime/handler_context.hpp"
#include "video/camera.hpp"
#include "video/camera_manager.hpp"

#include <algorithm>
#include <filesystem>
#include <opencv2/core.hpp>
#include <vector>

namespace fs = std::filesystem;

namespace service::stereo_calibration
{
namespace
{

/// @brief stereo calibration失敗resultを作成する。
///
/// Args:
///   output_file <std::filesystem::path>: stereo calibration結果の保存先file path。
///   code <std::string>: stereo calibration失敗時のerror code。
///   message <std::string>: stereo calibration失敗時のerror message。
///
/// Return:
///   <StereoCalibrationResult>: 失敗を表すstereo calibration result。
StereoCalibrationResult failure(fs::path output_file, std::string code, std::string message)
{
    StereoCalibrationResult result;
    result.output_file = std::move(output_file);
    result.error = StereoCalibrationError{std::move(code), std::move(message)};
    return result;
}

/// @brief directory内の通常file一覧を取得する。
///
/// Args:
///   directory <const std::filesystem::path&>: file一覧を取得するdirectory。
///
/// Return:
///   <std::vector<std::string>>: sort済みの通常file path一覧。
std::vector<std::string> listRegularFiles(const fs::path& directory)
{
    std::vector<std::string> files;
    for (const auto& entry : fs::directory_iterator(directory))
    {
        if (entry.is_regular_file())
        {
            files.push_back(entry.path().string());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

/// @brief calibration結果fileの親directoryを作成する。
///
/// Args:
///   output_file <const std::filesystem::path&>: 作成対象の親directoryを持つ出力file path。
///
/// Return:
///   <bool>: 親directoryが存在する、または作成できた場合はtrue。
bool ensureOutputParent(const fs::path& output_file)
{
    const auto parent = output_file.parent_path();
    if (parent.empty())
    {
        return true;
    }
    return fs::create_directories(parent) || fs::exists(parent);
}

} // namespace

StereoCalibrationResult calibrate(runtime::StereoCalibrationCalcContext& ctx, const cmd::CmdStereoCalibrate& command)
{
    const fs::path output_file{command.output_file};
    if (!ctx.stereo_calibrator)
    {
        return failure(output_file, "stereo_calibration_failed", "stereo calibrator is not available");
    }

    auto* cL = ctx.cameras.get(command.left_cam_id);
    auto* cR = ctx.cameras.get(command.right_cam_id);
    if (!cL || !cL->isOpened())
    {
        return failure(output_file, "camera_not_open", "left camera is not open");
    }
    if (!cR || !cR->isOpened())
    {
        return failure(output_file, "camera_not_open", "right camera is not open");
    }

    cv::Mat K1 = cL->intrinsics();
    cv::Mat D1 = cL->distCoeffs();
    cv::Mat K2 = cR->intrinsics();
    cv::Mat D2 = cR->distCoeffs();
    if (K1.empty() || D1.empty() || K2.empty() || D2.empty())
    {
        return failure(output_file, "stereo_calibration_failed", "left/right camera intrinsics are not ready");
    }

    const fs::path left_dir{command.left_dir};
    const fs::path right_dir{command.right_dir};
    if (!fs::exists(left_dir) || !fs::is_directory(left_dir))
    {
        return failure(output_file, "calibration_image_not_found", "left_dir does not exist");
    }
    if (!fs::exists(right_dir) || !fs::is_directory(right_dir))
    {
        return failure(output_file, "calibration_image_not_found", "right_dir does not exist");
    }

    const auto fL = listRegularFiles(left_dir);
    const auto fR = listRegularFiles(right_dir);
    if (fL.empty())
    {
        return failure(output_file, "calibration_image_not_found", "stereo calibration images are empty");
    }
    if (fL.size() != fR.size())
    {
        return failure(output_file, "calibration_image_count_mismatch", "left/right calibration image counts do not match");
    }

    LOG_INFO("Stereo: 計算開始 {} pairs", fL.size());
    calib::StereoData res;
    const double rms = ctx.stereo_calibrator->run(fL, fR, K1, D1, K2, D2, res);
    if (rms <= 0.0 || !res.valid)
    {
        return failure(output_file, "stereo_calibration_failed", "failed to run stereo calibration");
    }

    try
    {
        if (!ensureOutputParent(output_file))
        {
            return failure(output_file, "calibration_output_write_failed", "failed to create stereo calibration output directory");
        }
        cv::FileStorage fs_out(output_file.string(), cv::FileStorage::WRITE);
        if (!fs_out.isOpened())
        {
            return failure(output_file, "calibration_output_write_failed", "failed to open stereo calibration output file");
        }
        fs_out << "RMS" << rms << "K1" << K1 << "D1" << D1 << "K2" << K2 << "D2" << D2 << "R" << res.R
               << "T" << res.T << "Q" << res.Q;
    }
    catch (const std::exception& e)
    {
        return failure(output_file, "calibration_output_write_failed", e.what());
    }

    LOG_INFO("Stereo: 成功! RMS={}", rms);
    ctx.stereo_data = res;

    StereoCalibrationResult result;
    result.ok = true;
    result.rms = rms;
    result.output_file = output_file;
    return result;
}

StereoCalibrationResult calibrate(runtime::StereoCalibrationHandlerContext& ctx, const cmd::CmdStereoCalibrate& command)
{
    runtime::StereoCalibrationCalcContext calc_ctx{ctx.cameras, ctx.stereo_calibrator, ctx.stereo_data};
    return calibrate(calc_ctx, command);
}

} // namespace service::stereo_calibration
