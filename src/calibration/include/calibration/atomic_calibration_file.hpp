#pragma once

#include <filesystem>
#include <optional>

namespace calib::file
{

/**
 * @brief destinationと同じdirectoryに一意なCalibration用temporary pathを作る。
 *
 * 同一filesystem上でrenameできるよう、system temporary directoryは使用しない。
 * 返されたpathは呼び出し側が成功時にrenameし、失敗時に削除する。
 */
std::optional<std::filesystem::path> createTemporaryCalibrationPath(const std::filesystem::path& destination);

/// @brief temporary Calibration fileを削除し、失敗理由をerror_codeへ返す。
bool removeTemporaryCalibrationPath(const std::filesystem::path& path, std::error_code& error);

} // namespace calib::file
