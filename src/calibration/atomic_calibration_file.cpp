#include "calibration/atomic_calibration_file.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <unistd.h>

namespace service::calibration_file
{

std::optional<std::filesystem::path> createTemporaryCalibrationPath(const std::filesystem::path& destination)
{
    const auto parent = destination.parent_path().empty() ? std::filesystem::path{"."} : destination.parent_path();
    const auto stem = destination.filename().string() + ".XXXXXX.yml";
    std::string template_path = (parent / stem).string();
    std::array<char, 512> buffer{};
    if (template_path.size() + 1 > buffer.size())
    {
        return std::nullopt;
    }
    std::copy(template_path.begin(), template_path.end(), buffer.begin());
    buffer[template_path.size()] = '\0';
    const int file_descriptor = mkstemps(buffer.data(), 4);
    if (file_descriptor < 0)
    {
        return std::nullopt;
    }
    if (close(file_descriptor) != 0)
    {
        std::error_code cleanup_error;
        std::filesystem::remove(buffer.data(), cleanup_error);
        return std::nullopt;
    }
    return std::filesystem::path{buffer.data()};
}

bool removeTemporaryCalibrationPath(const std::filesystem::path& path, std::error_code& error)
{
    error.clear();
    return std::filesystem::remove(path, error) || !error;
}

} // namespace service::calibration_file
