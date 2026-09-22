#pragma once

#include "calibration/camera_projector_calibration.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace scan::dataset { class ScanDatasetValidator; }

namespace calib::projector
{
struct CalibrationConfig
{
    std::filesystem::path observations_dir;
    std::filesystem::path camera_calibration_file;
    std::filesystem::path output_file;
    cv::Size board_size;
    double square_size_mm{0.0};
    double max_mean_displacement_px{0.0};
    double max_corner_displacement_px{0.0};
    bool overwrite{false};
};

struct PoseDiagnostic
{
    std::string pose_name;
    bool accepted{false};
    std::string reason;
    double mean_corner_displacement_px{0.0};
    double max_corner_displacement_px{0.0};
};

struct ServiceResult
{
    bool ok{false};
    std::string error_code;
    std::string error;
    std::filesystem::path output_file;
    int total_pose_count{0};
    int accepted_pose_count{0};
    int rejected_pose_count{0};
    double projector_rms{-1.0};
    double stereo_rms{-1.0};
    int projector_width{0};
    int projector_height{0};
    std::vector<PoseDiagnostic> poses;
};

class CameraProjectorCalibrationService
{
  public:
    explicit CameraProjectorCalibrationService(const scan::dataset::ScanDatasetValidator& validator);
    ServiceResult calibrate(const CalibrationConfig& config) const;

  private:
    const scan::dataset::ScanDatasetValidator& validator_;
};
} // namespace calib::projector
