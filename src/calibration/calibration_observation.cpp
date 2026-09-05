#include "calibration/calibration_observation.hpp"

#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace calib
{
namespace
{

float distance(const cv::Point2f& lhs, const cv::Point2f& rhs)
{
    return std::hypot(lhs.x - rhs.x, lhs.y - rhs.y);
}

float areaRatio(const BoardObservation& observation, const cv::Size image_size)
{
    return image_size.area() > 0 ? (observation.size.width * observation.size.height) /
                                      static_cast<float>(image_size.area())
                                : 0.0F;
}

} // namespace

std::optional<BoardObservation> observeBoard(const cv::Mat& image, const cv::Size& pattern_size)
{
    if (image.empty() || pattern_size.width < 2 || pattern_size.height < 2)
    {
        return std::nullopt;
    }

    cv::Mat gray;
    if (image.channels() == 1)
    {
        gray = image;
    }
    else if (image.channels() == 3)
    {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    }
    else
    {
        return std::nullopt;
    }

    BoardObservation observation;
    const int flags = cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE | cv::CALIB_CB_FAST_CHECK;
    if (!cv::findChessboardCorners(gray, pattern_size, observation.corners, flags))
    {
        return observation;
    }

    cv::cornerSubPix(gray,
                     observation.corners,
                     cv::Size(11, 11),
                     cv::Size(-1, -1),
                     cv::TermCriteria(cv::TermCriteria::EPS | cv::TermCriteria::COUNT, 30, 0.01));

    observation.detected = true;
    cv::Rect2f bounds = cv::boundingRect(observation.corners);
    observation.center = (bounds.tl() + bounds.br()) * 0.5F;
    observation.size = bounds.size();
    const std::size_t last_row = static_cast<std::size_t>(pattern_size.height - 1) * pattern_size.width;
    const std::size_t last_column = static_cast<std::size_t>(pattern_size.width - 1);
    const auto& top_left = observation.corners.front();
    const auto& top_right = observation.corners[last_column];
    const auto& bottom_left = observation.corners[last_row];
    const auto& bottom_right = observation.corners[last_row + last_column];
    const float top_width = distance(top_left, top_right);
    const float bottom_width = distance(bottom_left, bottom_right);
    const float left_height = distance(top_left, bottom_left);
    const float right_height = distance(top_right, bottom_right);
    const float width_scale = std::max(top_width, bottom_width);
    const float height_scale = std::max(left_height, right_height);
    const float width_error = width_scale > 0.0F ? std::abs(top_width - bottom_width) / width_scale : 1.0F;
    const float height_error = height_scale > 0.0F ? std::abs(left_height - right_height) / height_scale : 1.0F;
    observation.perspective_error = std::max(width_error, height_error);
    if (observation.corners.size() >= 2)
    {
        const auto& first = observation.corners.front();
        const auto& next = observation.corners[1];
        observation.rotation_deg = static_cast<float>(std::atan2(next.y - first.y, next.x - first.x) * 180.0 / CV_PI);
    }

    cv::Mat laplacian;
    cv::Laplacian(gray, laplacian, CV_64F);
    cv::Scalar mean_value;
    cv::Scalar standard_deviation;
    cv::meanStdDev(laplacian, mean_value, standard_deviation);
    observation.blur_score = static_cast<float>(standard_deviation[0] * standard_deviation[0]);
    return observation;
}

bool meetsTarget(const BoardObservation& observation,
                 const BoardTarget& target,
                 const cv::Size& image_size,
                 const GuidanceThresholds& thresholds)
{
    if (!observation.detected || image_size.area() <= 0)
    {
        return false;
    }
    const cv::Point2f center_ratio(observation.center.x / image_size.width,
                                   observation.center.y / image_size.height);
    const float observed_area = areaRatio(observation, image_size);
    const float center_error = distance(center_ratio, target.center_ratio);
    return center_error <= thresholds.center_tolerance_ratio &&
           std::abs(observed_area - target.area_ratio) <= target.area_ratio * 0.35F &&
           observed_area >= thresholds.min_area_ratio && observed_area <= thresholds.max_area_ratio &&
           std::abs(observation.rotation_deg - target.rotation_deg) <= thresholds.rotation_tolerance_deg &&
           observation.perspective_error <= thresholds.perspective_tolerance &&
           observation.blur_score >= thresholds.minimum_blur_score;
}

StabilityResult evaluateStability(const std::vector<cv::Point2f>& previous,
                                  const std::vector<cv::Point2f>& current,
                                  const std::size_t previous_consecutive_frames,
                                  const GuidanceThresholds& thresholds)
{
    StabilityResult result;
    if (previous.size() != current.size() || current.empty())
    {
        return result;
    }

    float total = 0.0F;
    for (std::size_t index = 0; index < current.size(); ++index)
    {
        const float movement = distance(previous[index], current[index]);
        total += movement;
        result.max_motion_px = std::max(result.max_motion_px, movement);
    }
    result.mean_motion_px = total / static_cast<float>(current.size());
    const bool frame_is_stable = result.mean_motion_px <= thresholds.stability_mean_px &&
                                  result.max_motion_px <= thresholds.stability_max_px;
    result.consecutive_frames = frame_is_stable ? previous_consecutive_frames + 1 : 0;
    result.stable = result.consecutive_frames >= thresholds.stable_frames;
    return result;
}

BoardRange classifyRange(const float area_ratio, const GuidanceThresholds& thresholds)
{
    if (area_ratio <= 0.0F)
    {
        return BoardRange::unknown;
    }
    if (area_ratio < thresholds.min_area_ratio * 1.25F)
    {
        return BoardRange::far;
    }
    if (area_ratio > thresholds.max_area_ratio * 0.75F)
    {
        return BoardRange::near;
    }
    return BoardRange::middle;
}

void drawBoardObservation(cv::Mat& image, const BoardObservation& observation, const cv::Size& pattern_size)
{
    if (image.empty() || !observation.detected)
    {
        return;
    }
    cv::drawChessboardCorners(image, pattern_size, observation.corners, true);
    cv::rectangle(image, observation.center - cv::Point2f(observation.size.width, observation.size.height) * 0.5F,
                  observation.center + cv::Point2f(observation.size.width, observation.size.height) * 0.5F,
                  cv::Scalar(0, 255, 0), 2);
}

} // namespace calib
