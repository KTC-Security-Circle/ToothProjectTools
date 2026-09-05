#include "calibration/calibration_observation.hpp"

#include <cstdlib>
#include <iostream>

namespace
{

void require(const bool condition, const char* expression, const int line)
{
    if (!condition)
    {
        std::cerr << "requirement failed at line " << line << ": " << expression << '\n';
        std::abort();
    }
}

#define REQUIRE(expression) require(static_cast<bool>(expression), #expression, __LINE__)

void testStability()
{
    calib::GuidanceThresholds thresholds;
    thresholds.stability_mean_px = 0.5F;
    thresholds.stability_max_px = 1.0F;
    thresholds.stable_frames = 3;
    const std::vector<cv::Point2f> previous{{10.0F, 10.0F}, {20.0F, 10.0F}};
    const std::vector<cv::Point2f> current{{10.2F, 10.1F}, {20.2F, 10.1F}};

    const auto result = calib::evaluateStability(previous, current, 2, thresholds);
    REQUIRE(result.stable);
    REQUIRE(result.consecutive_frames == 3);
    REQUIRE(result.mean_motion_px < thresholds.stability_mean_px);

    const auto reset = calib::evaluateStability(previous, {{12.0F, 10.0F}, {20.0F, 10.0F}}, 2, thresholds);
    REQUIRE(!reset.stable);
    REQUIRE(reset.consecutive_frames == 0);
}

void testRangeClassification()
{
    calib::GuidanceThresholds thresholds;
    REQUIRE(calib::classifyRange(0.01F, thresholds) == calib::BoardRange::far);
    REQUIRE(calib::classifyRange(0.20F, thresholds) == calib::BoardRange::middle);
    REQUIRE(calib::classifyRange(0.50F, thresholds) == calib::BoardRange::near);
    REQUIRE(calib::classifyRange(0.0F, thresholds) == calib::BoardRange::unknown);
}

void testInvalidObservation()
{
    REQUIRE(!calib::observeBoard(cv::Mat{}, cv::Size(10, 7)).has_value());
    REQUIRE(!calib::observeBoard(cv::Mat::zeros(20, 20, CV_8UC4), cv::Size(10, 7)).has_value());
}

} // namespace

int main()
{
    testStability();
    testRangeClassification();
    testInvalidObservation();
    return 0;
}
