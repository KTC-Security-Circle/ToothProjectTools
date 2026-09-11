#include "reconstruction/camera_projector_service.hpp"

#include <opencv2/calib3d.hpp>
#include <opencv2/imgcodecs.hpp>
#include <cmath>
#include <fstream>

namespace reconstruction::camera_projector
{
namespace
{
bool finiteMat(const cv::Mat& value) { return !value.empty() && cv::checkRange(value, true, nullptr); }
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
    if (!finiteMat(projector_matrix) || !finiteMat(projector_distortion) || !finiteMat(t) || !validRotation(r) ||
        !std::isfinite(result.projector_rms) || !std::isfinite(result.stereo_rms))
    { result.error = "calibration result is non-finite or rotation is invalid"; return result; }
    result.ok = true; result.projector_matrix = projector_matrix; result.projector_distortion = projector_distortion;
    result.rotation_camera_to_projector = r; result.translation_camera_to_projector = t;
    return result;
}

bool saveCalibration(const std::filesystem::path& path, cv::Size camera_size, cv::Size projector_size,
                     const cv::Mat& camera_matrix, const cv::Mat& camera_distortion, const CalibrationResult& result,
                     double square_size_mm, std::string& error)
{
    if (!result.ok) { error = "cannot save invalid calibration"; return false; }
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

ReconstructionResult reconstructToPly(const cv::Mat& projector_x, const cv::Mat& projector_y, const cv::Mat& valid_mask,
                                       const cv::Mat& camera_image, cv::Size projector_size, const cv::Mat& camera_matrix,
                                       const cv::Mat& camera_distortion, const cv::Mat& projector_matrix,
                                       const cv::Mat& projector_distortion, const cv::Mat& rotation,
                                       const cv::Mat& translation, const std::filesystem::path& output,
                                       double min_depth_mm, double max_depth_mm, double max_reprojection_error_px)
{
    ReconstructionResult result; result.max_reprojection_error_px = max_reprojection_error_px;
    if (projector_x.empty() || projector_y.size() != projector_x.size() || valid_mask.size() != projector_x.size() ||
        !validRotation(rotation) || translation.total() != 3 || projector_size.empty()) { result.error = "invalid reconstruction input"; return result; }
    std::ofstream ply(output); if (!ply) { result.error = "failed to open PLY output"; return result; }
    struct Point { cv::Point3d p; cv::Vec3b color; }; std::vector<Point> points;
    for (int y = 0; y < projector_x.rows; ++y) for (int x = 0; x < projector_x.cols; ++x)
    {
        if (!valid_mask.at<unsigned char>(y, x)) continue; ++result.candidates;
        const double px = projector_x.type() == CV_32S ? projector_x.at<int>(y, x) : projector_x.at<float>(y, x);
        const double py = projector_y.type() == CV_32S ? projector_y.at<int>(y, x) : projector_y.at<float>(y, x);
        if (!std::isfinite(px) || !std::isfinite(py)) { ++result.rejected_nonfinite; continue; }
        std::vector<cv::Point2f> c{{static_cast<float>(x), static_cast<float>(y)}};
        std::vector<cv::Point2f> p{{static_cast<float>(px), static_cast<float>(py)}};
        std::vector<cv::Point2f> cu, pu;
        cv::undistortPoints(c, cu, camera_matrix, camera_distortion); cv::undistortPoints(p, pu, projector_matrix, projector_distortion);
        cv::Mat p2 = cv::Mat::zeros(3, 4, CV_64F);
        rotation.copyTo(p2(cv::Rect(0, 0, 3, 3)));
        translation.reshape(1, 3).copyTo(p2(cv::Rect(3, 0, 1, 3)));
        cv::Mat homogeneous;
        cv::triangulatePoints(cv::Mat::eye(3, 4, CV_64F), p2, cu, pu, homogeneous);
        const double w = homogeneous.at<double>(3); if (!std::isfinite(w) || std::abs(w) < 1e-12) { ++result.rejected_nonfinite; continue; }
        cv::Point3d point(homogeneous.at<double>(0)/w, homogeneous.at<double>(1)/w, homogeneous.at<double>(2)/w);
        const cv::Mat projector_point = rotation * (cv::Mat_<double>(3,1) << point.x, point.y, point.z) + translation;
        if (!std::isfinite(point.z) || point.z < min_depth_mm || point.z > max_depth_mm || projector_point.at<double>(2) <= 0) { ++result.rejected_depth; continue; }
        std::vector<cv::Point2f> reproj;
        cv::projectPoints(std::vector<cv::Point3d>{point}, cv::Mat::zeros(3,1,CV_64F),
                          cv::Mat::zeros(3,1,CV_64F), camera_matrix, camera_distortion,
                          reproj, cv::noArray(), 0);
        const double error = cv::norm(reproj[0] - c[0]);
        if (error > max_reprojection_error_px) { ++result.rejected_reprojection; continue; }
        cv::Vec3b color(255,255,255); if (!camera_image.empty() && camera_image.size() == projector_x.size()) color = camera_image.channels() == 3 ? camera_image.at<cv::Vec3b>(y,x) : cv::Vec3b(camera_image.at<unsigned char>(y,x), camera_image.at<unsigned char>(y,x), camera_image.at<unsigned char>(y,x));
        points.push_back({point,color});
    }
    ply << "ply\nformat ascii 1.0\nelement vertex " << points.size() << "\nproperty float x\nproperty float y\nproperty float z\nproperty uchar red\nproperty uchar green\nproperty uchar blue\nend_header\n";
    for (const auto& point : points) ply << point.p.x << ' ' << point.p.y << ' ' << point.p.z << ' ' << static_cast<int>(point.color[2]) << ' ' << static_cast<int>(point.color[1]) << ' ' << static_cast<int>(point.color[0]) << '\n';
    result.valid_points = static_cast<int>(points.size()); result.ok = ply.good() && !points.empty(); if (!result.ok && result.error.empty()) result.error = "no valid points or PLY write failed"; return result;
}
} // namespace reconstruction::camera_projector
