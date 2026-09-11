#pragma once

#include <opencv2/core.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace reconstruction::camera_projector
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

/**
 * @brief Cameraと480x270 logical Projectorの校正を解く。
 *
 * R/TはCamera座標の点をProjector座標へ変換する
 * X_projector = R * X_camera + T。
 */
CalibrationResult calibrate(const std::vector<CalibrationObservation>& observations,
                            cv::Size camera_size, cv::Size projector_size,
                            const cv::Mat& camera_matrix, const cv::Mat& camera_distortion,
                            double square_size_mm);

bool saveCalibration(const std::filesystem::path& path, cv::Size camera_size, cv::Size projector_size,
                     const cv::Mat& camera_matrix, const cv::Mat& camera_distortion,
                     const CalibrationResult& result, double square_size_mm, std::string& error);

struct ReconstructionResult
{
    bool ok{false};
    int candidates{0};
    int valid_points{0};
    int rejected_nonfinite{0};
    int rejected_depth{0};
    int rejected_reprojection{0};
    double max_reprojection_error_px{2.0};
    std::string error;
};

/**
 * @brief Camera pixelとdecoded Projector logical pixelを三角測量してPLYへ保存する。
 *
 * distortionを除去した座標に対しP_camera=[I|0]、P_projector=[R|T]を使用する。
 * invalid mask、非有限値、両装置の後方点、reprojection error超過は出力しない。
 */
ReconstructionResult reconstructToPly(const cv::Mat& projector_x, const cv::Mat& projector_y,
                                      const cv::Mat& valid_mask, const cv::Mat& camera_image,
                                      cv::Size projector_size, const cv::Mat& camera_matrix,
                                      const cv::Mat& camera_distortion, const cv::Mat& projector_matrix,
                                      const cv::Mat& projector_distortion, const cv::Mat& rotation,
                                      const cv::Mat& translation, const std::filesystem::path& output,
                                      double min_depth_mm, double max_depth_mm,
                                      double max_reprojection_error_px);

} // namespace reconstruction::camera_projector
