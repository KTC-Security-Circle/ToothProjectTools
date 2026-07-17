#include "service/calibration_service.hpp"

#include "calibration/calibrator.hpp"
#include "capture/capture_result.hpp"
#include "capture/capture_service.hpp"
#include "logger/logger_macros.hpp"
#include "runtime/handler_context.hpp"
#include "video/camera.hpp"
#include "video/camera_manager.hpp"
#include "window/window.hpp"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <opencv2/core.hpp>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace service::calibration
{
namespace
{

/// @brief mono calibration失敗resultを作成する。
///
/// Args:
///   output_file <std::filesystem::path>: calibration結果の保存先file path。
///   code <std::string>: mono calibration失敗時のerror code。
///   message <std::string>: mono calibration失敗時のerror message。
///
/// Return:
///   <MonoCalibrationResult>: 失敗を表すmono calibration result。
MonoCalibrationResult failure(fs::path output_file, std::string code, std::string message)
{
    MonoCalibrationResult result;
    result.output_file = std::move(output_file);
    result.error = MonoCalibrationError{std::move(code), std::move(message)};
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

void clear(runtime::CalibrationHandlerContext& ctx, win::Window& target_window, const cmd::CmdCalibClear& command)
{
    if (target_window.id() == ctx.preview_window_id && !command.target_directory.empty())
    {
        fs::remove_all(command.target_directory);
        fs::create_directories(command.target_directory);
        LOG_INFO("Calib: フォルダクリア {}", command.target_directory);
    }
}

void capture(runtime::CalibrationHandlerContext& ctx, win::Window& target_window,
             const cmd::CmdCalibCapture& command)
{
    if (command.camera_id == video::kInvalidCameraId)
    {
        return;
    }

    auto it = ctx.camera_windows.find(command.camera_id);
    if (it != ctx.camera_windows.end() && it->second != target_window.id())
    {
        return;
    }

    if (!fs::exists(command.target_directory))
    {
        fs::create_directories(command.target_directory);
    }

    auto cnt = std::distance(fs::directory_iterator(command.target_directory), fs::directory_iterator{});
    std::stringstream ss;
    ss << command.target_directory << "/" << command.prefix << std::setfill('0') << std::setw(3) << cnt << ".png";

    const auto result = ctx.capture_service.captureFrame(command.camera_id, ss.str());
    if (!result.ok && result.error)
    {
        LOG_ERROR("Calib Capture: failed code={} message={}",
                  capture::toString(result.error->code),
                  result.error->message);
        return;
    }

    if (auto* cam = ctx.cameras.get(command.camera_id))
    {
        LOG_INFO("Saved[{}]: {}", cam->name(), result.output_path.string());
    }
}

MonoCalibrationResult calibrate(runtime::MonoCalibrationCalcContext& ctx, const cmd::CmdCalibrate& command)
{
    const fs::path output_file{command.output_file};
    if (!ctx.calibrator)
    {
        return failure(output_file, "calibration_failed", "mono calibrator is not available");
    }

    const fs::path image_folder{command.image_folder};
    if (!fs::exists(image_folder) || !fs::is_directory(image_folder))
    {
        return failure(output_file, "calibration_image_not_found", "mono calibration image_folder does not exist");
    }

    const auto files = listRegularFiles(image_folder);
    if (files.empty())
    {
        return failure(output_file, "calibration_image_not_found", "mono calibration image_folder has no images");
    }

    cv::Mat K, D;
    const double rms = ctx.calibrator->runCalibration(files, K, D);
    if (rms <= 0.0 || rms >= 1.0 || K.empty() || D.empty())
    {
        return failure(output_file, "calibration_failed", "failed to run mono calibration");
    }

    try
    {
        if (!ensureOutputParent(output_file))
        {
            return failure(output_file, "calibration_output_write_failed", "failed to create mono calibration output directory");
        }
        cv::FileStorage fs_out(output_file.string(), cv::FileStorage::WRITE);
        if (!fs_out.isOpened())
        {
            return failure(output_file, "calibration_output_write_failed", "failed to open mono calibration output file");
        }
        fs_out << "RMS" << rms << "K" << K << "D" << D;
    }
    catch (const std::exception& e)
    {
        return failure(output_file, "calibration_output_write_failed", e.what());
    }

    if (command.apply_to_camera)
    {
        auto* cam = ctx.cameras.get(command.target_camera_id);
        if (!cam || !cam->isOpened())
        {
            return failure(output_file, "camera_not_open", "camera is not open");
        }
        cam->setIntrinsics(K);
        cam->setDistCoeffs(D);
    }
    LOG_INFO("Calib: 成功 RMS={}", rms);

    MonoCalibrationResult result;
    result.ok = true;
    result.rms = rms;
    result.output_file = output_file;
    return result;
}

MonoCalibrationResult calibrate(runtime::CalibrationHandlerContext& ctx, const cmd::CmdCalibrate& command)
{
    runtime::MonoCalibrationCalcContext calc_ctx{ctx.cameras, ctx.calibrator};
    return calibrate(calc_ctx, command);
}

} // namespace service::calibration
