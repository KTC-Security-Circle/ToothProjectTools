#include "calibration/projector_calibrator.hpp"
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

namespace calib::projector
{
std::vector<PosePlan> defaultPosePlan()
{
    const std::vector<std::string> orientations{"center_front", "center_tilt_left", "center_tilt_right", "center_tilt_up", "center_tilt_down", "left_top", "right_top", "left_bottom", "right_bottom"};
    std::vector<PosePlan> result;
    for (const auto& distance : {std::string{"near"}, std::string{"middle"}, std::string{"far"}})
        for (const auto& orientation : orientations) result.push_back({distance + "_" + orientation, distance, orientation});
    return result;
}

ObservationResult makeObservation(const cv::Mat& before, const cv::Mat& after, const cv::Mat& projector_x,
                                  const cv::Mat& projector_y, const cv::Mat& valid_mask, cv::Size board_size,
                                  double square_size_mm, double max_mean, double max_corner)
{
    ObservationResult result;
    if (before.empty() || after.empty() || projector_x.size() != before.size() || projector_y.size() != before.size() ||
        valid_mask.size() != before.size() || board_size.width <= 0 || board_size.height <= 0) { result.error = "invalid observation input"; return result; }
    cv::Mat bgray, agray; if (before.channels() == 1) bgray = before; else cv::cvtColor(before, bgray, cv::COLOR_BGR2GRAY); if (after.channels() == 1) agray = after; else cv::cvtColor(after, agray, cv::COLOR_BGR2GRAY);
    std::vector<cv::Point2f> b, a;
    const int flags = cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE;
    if (!cv::findChessboardCorners(bgray, board_size, b, flags) || !cv::findChessboardCorners(agray, board_size, a, flags) || b.size() != a.size()) { result.error = "checkerboard corner detection failed"; return result; }
    const auto criteria = cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.1); cv::cornerSubPix(bgray, b, {11,11}, {-1,-1}, criteria); cv::cornerSubPix(agray, a, {11,11}, {-1,-1}, criteria);
    double sum = 0.0; for (std::size_t i=0;i<b.size();++i) { const double d=cv::norm(b[i]-a[i]); sum += d; result.max_corner_displacement=std::max(result.max_corner_displacement,d); }
    result.mean_corner_displacement = sum / static_cast<double>(b.size()); if (result.mean_corner_displacement > max_mean || result.max_corner_displacement > max_corner) { result.error = "board moved during pattern capture"; return result; }
    result.observation.object_points.reserve(b.size()); result.observation.camera_points = b; result.observation.projector_points.reserve(b.size());
    for (const auto& corner : b)
    {
        std::vector<cv::Point2f> candidates;
        const int cx = cvRound(corner.x), cy = cvRound(corner.y);
        for (int y=std::max(0,cy-2); y<=std::min(projector_x.rows-1,cy+2); ++y) for (int x=std::max(0,cx-2); x<=std::min(projector_x.cols-1,cx+2); ++x)
            if (valid_mask.at<unsigned char>(y,x)) {
                const float px = projector_x.type() == CV_32S ? static_cast<float>(projector_x.at<int>(y,x)) : projector_x.at<float>(y,x);
                const float py = projector_y.type() == CV_32S ? static_cast<float>(projector_y.at<int>(y,x)) : projector_y.at<float>(y,x);
                candidates.emplace_back(px, py);
            }
        if (candidates.size() < 3) { result.error = "insufficient valid decoded neighbors"; return result; }
        std::nth_element(candidates.begin(), candidates.begin()+candidates.size()/2, candidates.end(), [](const auto& l,const auto& r){return l.x<r.x;}); const float px=candidates[candidates.size()/2].x;
        std::nth_element(candidates.begin(), candidates.begin()+candidates.size()/2, candidates.end(), [](const auto& l,const auto& r){return l.y<r.y;}); const float py=candidates[candidates.size()/2].y;
        result.observation.projector_points.emplace_back(px,py);
    }
    for (int y=0;y<board_size.height;++y) for (int x=0;x<board_size.width;++x) result.observation.object_points.emplace_back(static_cast<float>(x*square_size_mm),static_cast<float>(y*square_size_mm),0.0f);
    result.valid = true; return result;
}
} // namespace calib::projector
