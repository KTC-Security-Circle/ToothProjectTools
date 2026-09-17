#include "calibration/calibration_file.hpp"

#include <array>
#include <algorithm>
#include <cmath>

#include <opencv2/core/persistence.hpp>

namespace calib::file
{
namespace
{

bool isFloatMatrix(const cv::Mat& mat)
{
    return mat.channels() == 1 && (mat.depth() == CV_32F || mat.depth() == CV_64F);
}

bool hasSupportedDistortionCount(const cv::Mat& mat)
{
    constexpr std::array<int, 5> supported{4, 5, 8, 12, 14};
    return std::find(supported.begin(), supported.end(), static_cast<int>(mat.total())) != supported.end();
}

bool containsOnlyFiniteValues(const cv::Mat& mat)
{
    return cv::checkRange(mat, true, nullptr);
}

} // namespace

std::optional<MonoCalibrationFile> loadMonoCalibrationFile(
    const std::filesystem::path& path,
    std::string& error_message)
{
    error_message.clear();
    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
    {
        error_message = "mono calibration file does not exist: " + path.string();
        return std::nullopt;
    }
    if (!std::filesystem::is_regular_file(path, ec))
    {
        error_message = "mono calibration path is not a file: " + path.string();
        return std::nullopt;
    }

    try
    {
        cv::FileStorage storage(path.string(), cv::FileStorage::READ);
        if (!storage.isOpened())
        {
            error_message = "failed to open mono calibration file: " + path.string();
            return std::nullopt;
        }

        MonoCalibrationFile file;
        storage["K"] >> file.K;
        storage["D"] >> file.D;
        const auto rms_node = storage["RMS"];
        if (!rms_node.empty() && rms_node.isReal())
        {
            file.rms = static_cast<double>(rms_node);
        }
        else if (!rms_node.empty() && rms_node.isInt())
        {
            file.rms = static_cast<int>(rms_node);
        }
        const auto width_node = storage["image_width"];
        const auto height_node = storage["image_height"];
        const bool has_width = !width_node.empty();
        const bool has_height = !height_node.empty();
        if (has_width != has_height)
        {
            error_message = "mono calibration image size must contain both image_width and image_height: " + path.string();
            return std::nullopt;
        }
        if (has_width && (!width_node.isInt() || !height_node.isInt()))
        {
            error_message = "mono calibration image size must be integer values: " + path.string();
            return std::nullopt;
        }
        if (!width_node.empty() && width_node.isInt())
        {
            file.image_width = static_cast<int>(width_node);
        }
        if (!height_node.empty() && height_node.isInt())
        {
            file.image_height = static_cast<int>(height_node);
        }
        if (has_width && (file.image_width <= 0 || file.image_height <= 0))
        {
            error_message = "mono calibration image size must be positive: " + path.string();
            return std::nullopt;
        }
        if (!std::isfinite(file.rms) || file.rms <= 0.0)
        {
            error_message = "mono calibration RMS is missing or invalid: " + path.string();
            return std::nullopt;
        }

        if (file.K.empty())
        {
            error_message = "mono calibration file must contain non-empty K: " + path.string();
            return std::nullopt;
        }
        if (file.D.empty())
        {
            error_message = "mono calibration file must contain non-empty D: " + path.string();
            return std::nullopt;
        }
        if (file.K.rows != 3 || file.K.cols != 3 || !isFloatMatrix(file.K))
        {
            error_message = "mono calibration K must be 3x3 single-channel float matrix: " + path.string();
            return std::nullopt;
        }
        if (!isFloatMatrix(file.D) || !((file.D.rows == 1 && file.D.cols >= 1) || (file.D.cols == 1 && file.D.rows >= 1)))
        {
            error_message = "mono calibration D must be 1xN or Nx1 single-channel float vector: " + path.string();
            return std::nullopt;
        }
        if (!hasSupportedDistortionCount(file.D))
        {
            error_message = "mono calibration D has unsupported coefficient count: " + path.string();
            return std::nullopt;
        }
        if (!containsOnlyFiniteValues(file.K))
        {
            error_message = "mono calibration K contains NaN or Inf: " + path.string();
            return std::nullopt;
        }
        if (!containsOnlyFiniteValues(file.D))
        {
            error_message = "mono calibration D contains NaN or Inf: " + path.string();
            return std::nullopt;
        }
        if (file.K.depth() == CV_32F)
        {
            file.K.convertTo(file.K, CV_64F);
        }
        if (file.D.depth() == CV_32F)
        {
            file.D.convertTo(file.D, CV_64F);
        }
        return file;
    }
    catch (const cv::Exception&)
    {
        error_message = "failed to parse mono calibration file: " + path.string();
        return std::nullopt;
    }
    catch (const std::exception& error)
    {
        error_message = error.what();
        return std::nullopt;
    }
}

} // namespace calib::file
