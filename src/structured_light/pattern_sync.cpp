#include "structured_light/pattern_sync.hpp"
#include <opencv2/imgproc.hpp>

namespace structured_light::sync
{
RoiObservation observeRoi(const cv::Mat& image, const RoiSyncConfig& config, MarkerState previous_state)
{
    RoiObservation result;
    if (image.empty() || config.roi.width <= 0 || config.roi.height <= 0 || config.roi.x < 0 || config.roi.y < 0 ||
        config.roi.x + config.roi.width > image.cols || config.roi.y + config.roi.height > image.rows)
        return result;
    cv::Mat gray;
    if (image.channels() == 1) gray = image(config.roi);
    else cv::cvtColor(image(config.roi), gray, cv::COLOR_BGR2GRAY);
    result.mean_brightness = cv::mean(gray)[0];
    if (result.mean_brightness <= config.black_threshold) result.state = MarkerState::black;
    else if (result.mean_brightness >= config.white_threshold) result.state = MarkerState::white;
    else result.state = previous_state;
    return result;
}

std::string toString(MarkerState state)
{
    switch (state) { case MarkerState::black: return "black"; case MarkerState::white: return "white"; case MarkerState::undecided: return "undecided"; }
    return "undecided";
}
std::string toString(SyncSource source)
{
    switch (source) { case SyncSource::camera_roi: return "camera_roi"; case SyncSource::photodiode: return "photodiode"; case SyncSource::fixed_delay: return "fixed_delay"; }
    return "unknown";
}
} // namespace structured_light::sync
