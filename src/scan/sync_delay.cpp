#include "scan/sync_delay.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <opencv2/imgproc.hpp>

namespace scan
{
namespace
{
cv::Mat gray64(const cv::Mat& image)
{
    if (image.empty()) return {};
    cv::Mat gray;
    if (image.channels() == 1) gray = image;
    else cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    cv::Mat result;
    gray.convertTo(result, CV_64F);
    return result;
}

double percentile(const std::vector<double>& sorted, double fraction)
{
    if (sorted.empty()) return 0.0;
    const double position = fraction * static_cast<double>(sorted.size() - 1);
    const auto lower = static_cast<std::size_t>(std::floor(position));
    const auto upper = static_cast<std::size_t>(std::ceil(position));
    const double weight = position - static_cast<double>(lower);
    return sorted[lower] * (1.0 - weight) + sorted[upper] * weight;
}
} // namespace

std::optional<MeasurementModel> buildMeasurementModel(
    const cv::Mat& black_image, const cv::Mat& white_image, double minimum_contrast,
    std::size_t minimum_pixels)
{
    const auto black = gray64(black_image);
    const auto white = gray64(white_image);
    if (black.empty() || white.empty() || black.size() != white.size() || minimum_contrast <= 0.0)
        return std::nullopt;
    cv::Mat difference;
    cv::absdiff(white, black, difference);
    cv::Mat mask = difference >= minimum_contrast;
    const auto count = static_cast<std::size_t>(cv::countNonZero(mask));
    if (count < minimum_pixels) return std::nullopt;
    return MeasurementModel{black, white, mask, count};
}

double matchedPixelRatio(const MeasurementModel& model, const cv::Mat& image,
                         structured_light::sync::MarkerState expected)
{
    if (model.pixel_count == 0 || image.empty() || expected == structured_light::sync::MarkerState::undecided)
        return 0.0;
    const auto frame = gray64(image);
    if (frame.size() != model.black.size()) return 0.0;
    const cv::Mat midpoint = (model.black + model.white) * 0.5;
    const cv::Mat white_brighter = model.white > model.black;
    cv::Mat classified_white = (white_brighter & (frame > midpoint)) | (~white_brighter & (frame < midpoint));
    cv::Mat matched = expected == structured_light::sync::MarkerState::white
                          ? classified_white
                          : ~classified_white;
    matched &= model.mask;
    return static_cast<double>(cv::countNonZero(matched)) /
           static_cast<double>(model.pixel_count);
}

bool frameMatches(const MeasurementModel& model, const cv::Mat& frame,
                  structured_light::sync::MarkerState expected, double required_ratio,
                  double* matched_ratio)
{
    const double ratio = matchedPixelRatio(model, frame, expected);
    if (matched_ratio) *matched_ratio = ratio;
    return ratio >= required_ratio;
}

SyncDelayStatistics calculateSyncDelayStatistics(const std::vector<double>& delays_ms,
                                                  double safety_margin_ms)
{
    SyncDelayStatistics result;
    if (delays_ms.empty()) return result;
    auto sorted = delays_ms;
    std::sort(sorted.begin(), sorted.end());
    result.count = sorted.size();
    result.mean_ms = std::accumulate(sorted.begin(), sorted.end(), 0.0) / sorted.size();
    result.median_ms = percentile(sorted, 0.50);
    result.p95_ms = percentile(sorted, 0.95);
    result.p99_ms = percentile(sorted, 0.99);
    result.max_ms = sorted.back();
    result.recommended_guard_ms = static_cast<int>(std::ceil(result.p99_ms + safety_margin_ms));
    return result;
}

std::vector<structured_light::sync::MarkerState> measurementStateSequence(int transitions)
{
    std::vector<structured_light::sync::MarkerState> states;
    for (int index = 0; index < std::max(0, transitions); ++index)
        states.push_back(index % 2 == 0 ? structured_light::sync::MarkerState::white
                                       : structured_light::sync::MarkerState::black);
    return states;
}

} // namespace scan
