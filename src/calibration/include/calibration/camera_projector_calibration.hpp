#pragma once

#include <filesystem>
#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace calib::projector
{
struct CalibrationObservation
{
    std::vector<cv::Point3f> object_points;
    std::vector<cv::Point2f> camera_points;
    std::vector<cv::Point2f> projector_points;
};

struct CalibrationResult
{
    bool ok{false};
    double projector_rms{-1.0};
    double stereo_rms{-1.0};
    cv::Mat projector_matrix;
    cv::Mat projector_distortion;
    cv::Mat rotation_camera_to_projector;
    cv::Mat translation_camera_to_projector;
    std::string error;
};

struct ProjectorSurface
{
    int pattern_x{0};
    int pattern_y{0};
    int pattern_width{0};
    int pattern_height{0};
};

/**
 * Cameraと480x270 logical Projectorの校正を解く。
 * X_projector = R_camera_to_projector * X_camera + T_camera_to_projector。
 * Tの単位はobject pointと同じで、public serviceではmm。
 */
CalibrationResult calibrate(const std::vector<CalibrationObservation>& observations,
                            cv::Size camera_size, cv::Size projector_size,
                            const cv::Mat& camera_matrix, const cv::Mat& camera_distortion,
                            double square_size_mm);

bool saveCalibration(const std::filesystem::path& path, cv::Size camera_size, cv::Size projector_size,
                     const ProjectorSurface& surface, cv::Size board_size,
                     const cv::Mat& camera_matrix, const cv::Mat& camera_distortion,
                     const CalibrationResult& result, double square_size_mm, std::string& error);
} // namespace calib::projector
