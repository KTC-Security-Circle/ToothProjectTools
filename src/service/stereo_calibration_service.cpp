#include "service/stereo_calibration_service.hpp"

#include "service/atomic_calibration_file.hpp"
#include "service/calibration_file.hpp"

#include "calibration/stereo_calibrator.hpp"
#include "calibration/stereo_data.hpp"
#include "logger/logger_macros.hpp"
#include "video/camera.hpp"
#include "video/camera_manager.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <opencv2/core.hpp>
#include <opencv2/core/base.hpp>
#include <opencv2/imgcodecs.hpp>
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

bool validStereoResult(const calib::StereoData& data, double rms, cv::Size image_size)
{
    constexpr double kRotationTolerance = 1e-4;
    if (!data.valid || data.R.rows != 3 || data.R.cols != 3)
    {
        return false;
    }
    cv::Mat rotation64;
    data.R.convertTo(rotation64, CV_64F);
    const double orthogonality_error = cv::norm(rotation64.t() * rotation64 - cv::Mat::eye(3, 3, CV_64F), cv::NORM_INF);
    const double determinant = cv::determinant(rotation64);
    return data.valid && image_size.width > 0 && image_size.height > 0 && std::isfinite(rms) && rms > 0.0 &&
           data.R.rows == 3 && data.R.cols == 3 && data.T.rows == 3 && data.T.cols == 1 &&
           data.Q.rows == 4 && data.Q.cols == 4 && cv::checkRange(data.R) && cv::checkRange(data.T) &&
           cv::checkRange(data.Q) && std::isfinite(orthogonality_error) &&
           orthogonality_error <= kRotationTolerance && std::isfinite(determinant) &&
           std::abs(determinant - 1.0) <= kRotationTolerance;
}

std::optional<cv::Size> firstImageSize(const std::vector<std::string>& files)
{
    for (const auto& file : files)
    {
        const cv::Mat image = cv::imread(file, cv::IMREAD_UNCHANGED);
        if (!image.empty())
        {
            return image.size();
        }
    }
    return std::nullopt;
}

} // namespace

StereoCalibrationResult calibrate(video::CameraManager& cameras, calib::StereoCalibrator* stereo_calibrator,
                                  calib::StereoData& stereo_data, const cmd::CmdStereoCalibrate& command)
{
    LOG_INFO("step1: Stereo: 計算要求 left={} right={} left_dir={} right_dir={} output_file={}",
             command.left_cam_id, command.right_cam_id, command.left_dir, command.right_dir, command.output_file);
    const fs::path output_file{command.output_file};
    if (!stereo_calibrator)
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
        auto* cL = cameras.get(command.left_cam_id);
        auto* cR = cameras.get(command.right_cam_id);
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

    const auto left_image_size = firstImageSize(fL);
    const auto right_image_size = firstImageSize(fR);
    if (!left_image_size || !right_image_size || *left_image_size != *right_image_size)
    {
        return failure(output_file, "stereo_calibration_failed", "stereo image sizes are missing or inconsistent");
    }
    if (left_calibration->image_width > 0 && left_calibration->image_height > 0 &&
        (left_calibration->image_width != left_image_size->width ||
         left_calibration->image_height != left_image_size->height))
    {
        return failure(output_file, "calibration_image_size_mismatch", "left mono calibration image size does not match stereo images");
    }
    if (right_calibration->image_width > 0 && right_calibration->image_height > 0 &&
        (right_calibration->image_width != right_image_size->width ||
         right_calibration->image_height != right_image_size->height))
    {
        return failure(output_file, "calibration_image_size_mismatch", "right mono calibration image size does not match stereo images");
    }
    if (left_calibration->image_width > 0 && right_calibration->image_width > 0 &&
        (left_calibration->image_width != right_calibration->image_width ||
         left_calibration->image_height != right_calibration->image_height))
    {
        return failure(output_file, "calibration_image_size_mismatch", "left/right mono calibration image sizes differ");
    }

    LOG_INFO("Stereo: 計算開始 {} pairs", fL.size());
    calib::StereoData res;
    double rms = 0.0;
    try
    {
        rms = stereo_calibrator->run(fL, fR, K1, D1, K2, D2, res);
    }
    catch (const cv::Exception& error)
    {
        return failure(output_file, "stereo_calibration_failed", error.what());
    }
    catch (const std::exception& error)
    {
        return failure(output_file, "stereo_calibration_failed", error.what());
    }
    const cv::Size image_size = res.mapL_x.size();
    if (!validStereoResult(res, rms, image_size))
    {
        return failure(output_file, "stereo_calibration_failed", "failed to run stereo calibration");
    }

    std::optional<fs::path> temporary_file;
    try
    {
        if (!ensureOutputParent(output_file))
        {
            return failure(output_file, "file_write_failed", "failed to create stereo calibration output directory");
        }
        const auto temporary_path = calibration_file::createTemporaryCalibrationPath(output_file);
        if (!temporary_path)
        {
            return failure(output_file, "file_write_failed", "failed to create temporary calibration file");
        }
        temporary_file = *temporary_path;
        cv::FileStorage fs_out(temporary_file->string(), cv::FileStorage::WRITE);
        if (!fs_out.isOpened())
        {
            std::error_code cleanup_error;
            if (!calibration_file::removeTemporaryCalibrationPath(*temporary_file, cleanup_error))
            {
                LOG_WARN("Stereo Calibration temporary file cleanup failed: {}", cleanup_error.message());
            }
            return failure(output_file, "file_write_failed", "failed to open stereo calibration output file");
        }
        fs_out << "version" << "0.1.0" << "image_width" << image_size.width << "image_height" << image_size.height << "RMS" << rms << "K1" << K1 << "D1" << D1 << "K2" << K2 << "D2" << D2 << "R" << res.R
               << "T" << res.T << "Q" << res.Q;
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
                LOG_WARN("Stereo Calibration temporary file cleanup failed: {}", cleanup_error.message());
            }
        }
        return failure(output_file, "file_write_failed", e.what());
    }

    LOG_INFO("Stereo: 成功! RMS={}", rms);
    stereo_data = res;

    StereoCalibrationResult result;
    result.ok = true;
    result.rms = rms;
    result.output_file = output_file;
    return result;
}

} // namespace service::stereo_calibration
