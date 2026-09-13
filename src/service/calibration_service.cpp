#include "service/calibration_service.hpp"
#include "service/atomic_calibration_file.hpp"

#include "calibration/calibrator.hpp"
#include "capture/capture_result.hpp"
#include "capture/capture_service.hpp"
#include "logger/logger_macros.hpp"
#include "runtime/handler_context.hpp"
#include "video/camera.hpp"
#include "video/camera_manager.hpp"
#include "video/video_types.hpp"
#include "window/window.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
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

/// @brief 単眼結果が後段の復元で意味を持つ値か検証する。
bool validMonoResult(const cv::Mat& camera_matrix, const cv::Mat& dist_coeffs, double rms, cv::Size image_size)
{
    if (camera_matrix.rows != 3 || camera_matrix.cols != 3 || camera_matrix.channels() != 1 ||
        dist_coeffs.empty() || image_size.width <= 0 || image_size.height <= 0 || !std::isfinite(rms) ||
        rms <= 0.0 || rms >= 1.0 || !cv::checkRange(camera_matrix) || !cv::checkRange(dist_coeffs))
    {
        return false;
    }

    cv::Mat matrix64;
    camera_matrix.convertTo(matrix64, CV_64F);
    const double fx = matrix64.at<double>(0, 0);
    const double fy = matrix64.at<double>(1, 1);
    const double cx = matrix64.at<double>(0, 2);
    const double cy = matrix64.at<double>(1, 2);
    return fx > 0.0 && fy > 0.0 && cx >= 0.0 && cx < image_size.width && cy >= 0.0 && cy < image_size.height;
}

std::optional<common::CommandResult> writePreview(const fs::path& path, const cv::Mat& image)
{
    try
    {
        if (!ensureOutputParent(path) || !cv::imwrite(path.string(), image))
            return common::failure("calibration_output_write_failed", "failed to write corner preview image");
    }
    catch (const std::exception& error)
    {
        return common::failure("calibration_output_write_failed", error.what());
    }
    return std::nullopt;
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

    ctx.calibrator->setBoardConfig(command.board_config);
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
    cv::Size image_size;
    for (const auto& file : files)
    {
        const cv::Mat image = cv::imread(file, cv::IMREAD_UNCHANGED);
        if (!image.empty())
        {
            image_size = image.size();
            break;
        }
    }
    if (!validMonoResult(K, D, rms, image_size))
    {
        return failure(output_file, "calibration_failed", "failed to run mono calibration");
    }

    std::optional<fs::path> temporary_file;
    try
    {
        if (!ensureOutputParent(output_file))
        {
            return failure(output_file, "calibration_output_write_failed", "failed to create mono calibration output directory");
        }
        const auto temporary_path = calibration_file::createTemporaryCalibrationPath(output_file);
        if (!temporary_path)
        {
            return failure(output_file, "calibration_output_write_failed", "failed to create temporary calibration file");
        }
        temporary_file = *temporary_path;
        cv::FileStorage fs_out(temporary_file->string(), cv::FileStorage::WRITE);
        if (!fs_out.isOpened())
        {
            std::error_code cleanup_error;
            if (!calibration_file::removeTemporaryCalibrationPath(*temporary_file, cleanup_error))
            {
                LOG_WARN("Calibration temporary file cleanup failed: {}", cleanup_error.message());
            }
            return failure(output_file, "calibration_output_write_failed", "failed to open mono calibration output file");
        }
        fs_out << "RMS" << rms << "image_width" << image_size.width << "image_height" << image_size.height
               << "board_corners_x" << command.board_config.pattern_size.width
               << "board_corners_y" << command.board_config.pattern_size.height
               << "square_size_mm" << command.board_config.square_size_mm
               << "K" << K << "D" << D;
        fs_out.release();
        fs::rename(*temporary_file, output_file);
    }
    catch (const std::exception& e)
    {
        if (temporary_file)
        {
            std::error_code cleanup_error;
            if (!calibration_file::removeTemporaryCalibrationPath(*temporary_file, cleanup_error))
            {
                LOG_WARN("Calibration temporary file cleanup failed: {}", cleanup_error.message());
            }
        }
        LOG_ERROR("Calibration output write failed: {}", e.what());
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

common::CommandResult detectCorners(runtime::MonoCalibrationCalcContext& ctx,
                                    const cmd::CmdDetectCalibrationCorners& command)
{
    if (!ctx.calibrator) return common::failure("calibration_failed", "mono calibrator is not available");
    auto* camera = ctx.cameras.get(command.camera_id);
    if (!camera || !camera->isOpened()) return common::failure("camera_not_open", "camera is not open");
    const auto sample = camera->getFrameSample();
    if (!sample || sample->image.empty()) return common::failure("empty_frame", "valid camera frame is not available");
    ctx.calibrator->setBoardConfig(command.board_config);
    cv::Mat preview;
    std::vector<cv::Point2f> corners;
    const bool found = ctx.calibrator->detectAndDraw(sample->image, preview, corners);
    if (const auto failure = writePreview(command.output_path, preview)) return *failure;
    return common::success({{"role", command.role}, {"found", found ? "true" : "false"},
                            {"corner_count", std::to_string(found ? corners.size() : 0)},
                            {"expected_corner_count", std::to_string(command.board_config.pattern_size.area())},
                            {"path", command.output_path.string()}});
}

common::CommandResult detectStereoCorners(runtime::MonoCalibrationCalcContext& ctx,
                                          const cmd::CmdDetectStereoCalibrationCorners& command)
{
    if (!ctx.calibrator) return common::failure("calibration_failed", "mono calibrator is not available");
    auto* left = ctx.cameras.get(command.left_camera_id);
    auto* right = ctx.cameras.get(command.right_camera_id);
    if (!left || !right || !left->isOpened() || !right->isOpened())
        return common::failure("camera_not_open", "left or right camera is not open");
    const auto left_sample = left->getFrameSample();
    const auto right_sample = right->getFrameSample();
    if (!left_sample || !right_sample || left_sample->image.empty() || right_sample->image.empty())
        return common::failure("empty_frame", "valid left or right camera frame is not available");

    ctx.calibrator->setBoardConfig(command.board_config);
    cv::Mat left_preview, right_preview;
    std::vector<cv::Point2f> left_corners, right_corners;
    const bool left_found = ctx.calibrator->detectAndDraw(left_sample->image, left_preview, left_corners);
    const bool right_found = ctx.calibrator->detectAndDraw(right_sample->image, right_preview, right_corners);
    if (const auto failure = writePreview(command.left_output_path, left_preview)) return *failure;
    if (const auto failure = writePreview(command.right_output_path, right_preview)) return *failure;
    return common::success({{"left_found", left_found ? "true" : "false"},
                            {"right_found", right_found ? "true" : "false"},
                            {"both_found", left_found && right_found ? "true" : "false"},
                            {"left_corner_count", std::to_string(left_found ? left_corners.size() : 0)},
                            {"right_corner_count", std::to_string(right_found ? right_corners.size() : 0)},
                            {"expected_corner_count", std::to_string(command.board_config.pattern_size.area())},
                            {"left_path", command.left_output_path.string()},
                            {"right_path", command.right_output_path.string()}});
}

MonoCalibrationResult calibrate(runtime::CalibrationHandlerContext& ctx, const cmd::CmdCalibrate& command)
{
    runtime::MonoCalibrationCalcContext calc_ctx{ctx.cameras, ctx.calibrator};
    return calibrate(calc_ctx, command);
}

} // namespace service::calibration
