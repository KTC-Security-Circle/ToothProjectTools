#include "reconstruction/camera_projector_service.hpp"

#include <opencv2/calib3d.hpp>
#include <cmath>

namespace reconstruction::camera_projector
{
namespace
{
bool finiteMat(const cv::Mat& value) { return !value.empty() && cv::checkRange(value, true, nullptr); }
bool validBaseline(const cv::Mat& translation) { return finiteMat(translation) && cv::norm(translation) > 1e-12; }
bool validRotation(const cv::Mat& r)
{
    if (r.rows != 3 || r.cols != 3 || !finiteMat(r)) return false;
    const double det = cv::determinant(r);
    return cv::norm(r.t() * r - cv::Mat::eye(3, 3, r.type())) < 1e-3 && std::abs(det - 1.0) < 1e-3;
}
}

CalibrationResult calibrate(const std::vector<CalibrationObservation>& observations, cv::Size camera_size,
                            cv::Size projector_size, const cv::Mat& camera_matrix,
                            const cv::Mat& camera_distortion, double square_size_mm)
{
    CalibrationResult result;
    if (observations.size() < 3 || camera_size.empty() || projector_size != cv::Size{480, 270} ||
        camera_matrix.rows != 3 || camera_matrix.cols != 3 || square_size_mm <= 0.0)
    { result.error = "invalid calibration input"; return result; }
    std::vector<std::vector<cv::Point3f>> objects;
    std::vector<std::vector<cv::Point2f>> cameras, projectors;
    for (const auto& observation : observations)
    {
        if (observation.object_points.size() < 10 || observation.object_points.size() != observation.camera_points.size() ||
            observation.object_points.size() != observation.projector_points.size()) continue;
        objects.push_back(observation.object_points); cameras.push_back(observation.camera_points);
        projectors.push_back(observation.projector_points);
    }
    if (objects.size() < 3) { result.error = "fewer than three valid poses"; return result; }
    cv::Mat projector_matrix = cv::initCameraMatrix2D(objects, projectors, projector_size, 0.0);
    cv::Mat projector_distortion = cv::Mat::zeros(1, 5, CV_64F), rvecs, tvecs;
    result.projector_rms = cv::calibrateCamera(objects, projectors, projector_size, projector_matrix,
                                               projector_distortion, rvecs, tvecs);
    cv::Mat camera_k = camera_matrix.clone(), camera_d = camera_distortion.clone();
    cv::Mat r, t, e, f;
    result.stereo_rms = cv::stereoCalibrate(objects, cameras, projectors, camera_k, camera_d,
                                            projector_matrix, projector_distortion, camera_size, r, t, e, f,
                                            cv::CALIB_FIX_INTRINSIC,
                                            cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, 100, 1e-7));
    if (!finiteMat(projector_matrix) || !finiteMat(projector_distortion) || !validBaseline(t) || !validRotation(r) ||
        !std::isfinite(result.projector_rms) || !std::isfinite(result.stereo_rms))
    { result.error = "calibration result is non-finite, zero-baseline, or rotation is invalid"; return result; }
    result.ok = true; result.projector_matrix = projector_matrix; result.projector_distortion = projector_distortion;
    result.rotation_camera_to_projector = r; result.translation_camera_to_projector = t;
    return result;
}

bool saveCalibration(const std::filesystem::path& path, cv::Size camera_size, cv::Size projector_size,
                     const cv::Mat& camera_matrix, const cv::Mat& camera_distortion, const CalibrationResult& result,
                     double square_size_mm, std::string& error)
{
    if (!result.ok || !validBaseline(result.translation_camera_to_projector)) { error = "cannot save invalid calibration"; return false; }
    try {
        cv::FileStorage storage(path.string(), cv::FileStorage::WRITE);
        if (!storage.isOpened()) { error = "failed to open calibration output"; return false; }
        storage << "mode" << "camera_projector" << "board_corners_x" << 10 << "board_corners_y" << 7
                << "square_size_mm" << square_size_mm;
        storage << "camera_width" << camera_size.width << "camera_height" << camera_size.height
                << "projector_width" << projector_size.width << "projector_height" << projector_size.height;
        storage << "camera_K" << camera_matrix << "camera_D" << camera_distortion
                << "projector_K" << result.projector_matrix << "projector_D" << result.projector_distortion
                << "R_camera_to_projector" << result.rotation_camera_to_projector
                << "T_camera_to_projector" << result.translation_camera_to_projector
                << "projector_rms" << result.projector_rms << "stereo_rms" << result.stereo_rms;
        storage.release(); return true;
    } catch (const cv::Exception& exception) { error = exception.what(); return false; }
}
} // namespace reconstruction::camera_projector
