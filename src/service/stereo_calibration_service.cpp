#include "service/stereo_calibration_service.hpp"

#include "service/calibration_file.hpp"

#include "calibration/stereo_calibrator.hpp"
#include "calibration/stereo_data.hpp"
#include "logger/logger_macros.hpp"
#include "runtime/handler_context.hpp"
#include "video/camera.hpp"
#include "video/camera_manager.hpp"

#include <algorithm>
#include <filesystem>
#include <opencv2/core.hpp>
#include <opencv2/core/base.hpp>
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
    LOG_INFO("step1: Stereo: 計算要求 left={} right={} left_dir={} right_dir={} output_file={}",
             command.left_cam_id, command.right_cam_id, command.left_dir, command.right_dir, command.output_file);
    const fs::path output_file{command.output_file};
    if (!ctx.stereo_calibrator)
    {
        return failure(output_file, "stereo_calibration_failed", "stereo calibrator is not available");
    }
    LOG_INFO("step2: Stereo: mono calibration file読込");

    std::string calibration_error;
    const auto left_calibration = service::calibration_file::loadMonoCalibrationFile(command.left_calibration_file, calibration_error);
    if (!left_calibration)
    {
        const auto code = fs::exists(command.left_calibration_file) ? "calibration_file_invalid" : "calibration_file_not_found";
        return failure(output_file, code, calibration_error);
    }
    const auto right_calibration = service::calibration_file::loadMonoCalibrationFile(command.right_calibration_file, calibration_error);
    if (!right_calibration)
    {
        const auto code = fs::exists(command.right_calibration_file) ? "calibration_file_invalid" : "calibration_file_not_found";
        return failure(output_file, code, calibration_error);
    }

    cv::Mat K1 = left_calibration->K;
    cv::Mat D1 = left_calibration->D;
    cv::Mat K2 = right_calibration->K;
    cv::Mat D2 = right_calibration->D;

    if (command.apply_to_camera)
    {
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
    }
    LOG_INFO("step4: Stereo: 左右画像pairの取得");

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
    LOG_INFO("step5: Stereo: 左右画像pairの計算");


    const auto fL = listRegularFiles(left_dir);
    const auto fR = listRegularFiles(right_dir);
    if (fL.empty())
    {
        return failure(output_file, "calibration_image_not_found", "stereo calibration images are empty");
    }
    if (fL.size() != fR.size())
    {
        return failure(output_file, "stereo_image_pair_mismatch", "left/right calibration image counts do not match");
    }

    LOG_INFO("Stereo: 計算開始 {} pairs", fL.size());
    calib::StereoData res;
    double rms = 0.0;
    try
    {
        rms = ctx.stereo_calibrator->run(fL, fR, K1, D1, K2, D2, res);
    }
    catch (const cv::Exception& error)
    {
        return failure(output_file, "stereo_calibration_failed", error.what());
    }
    catch (const std::exception& error)
    {
        return failure(output_file, "stereo_calibration_failed", error.what());
    }
    if (rms <= 0.0 || !res.valid)
    {
        return failure(output_file, "stereo_calibration_failed", "failed to run stereo calibration");
    }

    try
    {
        if (!ensureOutputParent(output_file))
        {
            return failure(output_file, "file_write_failed", "failed to create stereo calibration output directory");
        }
        cv::FileStorage fs_out(output_file.string(), cv::FileStorage::WRITE);
        if (!fs_out.isOpened())
        {
            return failure(output_file, "file_write_failed", "failed to open stereo calibration output file");
        }
        fs_out << "version" << "0.1.0" << "image_width" << res.mapL_x.cols << "image_height" << res.mapL_x.rows << "RMS" << rms << "K1" << K1 << "D1" << D1 << "K2" << K2 << "D2" << D2 << "R" << res.R
               << "T" << res.T << "Q" << res.Q;
    }
    catch (const std::exception& e)
    {
        return failure(output_file, "file_write_failed", e.what());
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
    LOG_INFO("step0: Stereo: 計算要求 left={} right={} left_dir={} right_dir={} output_file={}",
             command.left_cam_id, command.right_cam_id, command.left_dir, command.right_dir, command.output_file);
    runtime::StereoCalibrationCalcContext calc_ctx{ctx.cameras, ctx.stereo_calibrator, ctx.stereo_data};
    return calibrate(calc_ctx, command);
}

} // namespace service::stereo_calibration
