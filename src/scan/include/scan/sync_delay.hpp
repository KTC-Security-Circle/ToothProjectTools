#pragma once

#include "structured_light/pattern_sync.hpp"
#include "video/video_types.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace scan
{

struct SyncDelayConfig
{
    std::string projector_role;
    std::string camera_role;
    std::string photodiode_device{"/dev/ttyUSB0"};
    int photodiode_baud{115200};
    int transitions{60};
    int sync_timeout_ms{1000};
    double safety_margin_ms{5.0};
    double minimum_contrast{30.0};
    double required_ratio{0.90};
    std::filesystem::path output_csv{"data/photodiode_delay.csv"};
};

struct SyncDelayMeasurement
{
    int sequence{0};
    structured_light::sync::MarkerState state{structured_light::sync::MarkerState::undecided};
    std::chrono::steady_clock::time_point photodiode_timestamp{};
    std::chrono::steady_clock::time_point camera_timestamp{};
    double delay_ms{0.0};
    std::uint64_t frame_sequence{0};
    double matched_ratio{0.0};
};

struct SyncDelayStatistics
{
    std::size_t count{0};
    double mean_ms{0.0};
    double median_ms{0.0};
    double p95_ms{0.0};
    double p99_ms{0.0};
    double max_ms{0.0};
    int recommended_guard_ms{0};
};

struct SyncDelayResult
{
    bool ok{false};
    std::string error_code;
    std::string error_message;
    SyncDelayStatistics statistics;
    std::vector<SyncDelayMeasurement> measurements;
    std::size_t measurement_pixel_count{0};
    std::string csv_path;
    std::string csv_warning;
};

struct MeasurementModel
{
    cv::Mat black;
    cv::Mat white;
    cv::Mat mask;
    std::size_t pixel_count{0};
};

std::optional<MeasurementModel> buildMeasurementModel(
    const cv::Mat& black, const cv::Mat& white, double minimum_contrast,
    std::size_t minimum_pixels = 32);
double matchedPixelRatio(const MeasurementModel& model, const cv::Mat& frame,
                         structured_light::sync::MarkerState expected);
bool frameMatches(const MeasurementModel& model, const cv::Mat& frame,
                  structured_light::sync::MarkerState expected, double required_ratio,
                  double* matched_ratio = nullptr);
SyncDelayStatistics calculateSyncDelayStatistics(const std::vector<double>& delays_ms,
                                                  double safety_margin_ms);
std::vector<structured_light::sync::MarkerState> measurementStateSequence(int transitions);

} // namespace scan
