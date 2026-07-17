#include "service/calibration_file.hpp"

#include <opencv2/core/persistence.hpp>

namespace service::calibration_file
{

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

        if (file.K.empty() || file.D.empty())
        {
            error_message = "mono calibration file must contain non-empty K and D: " + path.string();
            return std::nullopt;
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

} // namespace service::calibration_file
