#pragma once

#include "reconstruction/camera_projector_service.hpp"
#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace calib::projector
{
struct PosePlan { std::string name; std::string distance; std::string orientation; };

/** @brief near/middle/far各9姿勢のProjector calibration計画を生成する。 */
std::vector<PosePlan> defaultPosePlan();

struct ObservationResult
{
    bool valid{false};
    double mean_corner_displacement{0.0};
    double max_corner_displacement{0.0};
    reconstruction::camera_projector::CalibrationObservation observation;
    std::string error;
};

/**
 * @brief Before/Afterのboard移動を検査し、subpixel cornerへdecode mapを結合する。
 *
 * Projector座標はCamera pixelの周囲5x5からvalid pixelだけを集め、中央値で推定する。
 */
ObservationResult makeObservation(const cv::Mat& reference_before, const cv::Mat& reference_after,
                                  const cv::Mat& projector_x, const cv::Mat& projector_y,
                                  const cv::Mat& valid_mask, cv::Size board_size, double square_size_mm,
                                  double max_mean_displacement_px, double max_corner_displacement_px);
} // namespace calib::projector
