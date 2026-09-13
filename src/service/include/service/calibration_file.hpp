#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include <opencv2/core.hpp>

namespace service::calibration_file
{

struct MonoCalibrationFile
{
    cv::Mat K;
    cv::Mat D;
    double rms{0.0};
    int image_width{0};
    int image_height{0};
};

std::optional<MonoCalibrationFile> loadMonoCalibrationFile(
    const std::filesystem::path& path,
    std::string& error_message);

} // namespace service::calibration_file
