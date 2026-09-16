#include "scan/camera_roi_sync_tracker.hpp"

#include <chrono>
#include <opencv2/core.hpp>
#include <stdexcept>
#include <string>

namespace
{

using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
using structured_light::sync::MarkerState;

void require(bool condition, const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

video::FrameSample sample(std::uint64_t sequence, Clock::time_point timestamp, int brightness,
                          int width = 8, int height = 8)
{
    return {cv::Mat(height, width, CV_8UC1, cv::Scalar(brightness)), sequence, timestamp};
}

scan::CameraRoiSyncTracker tracker(std::chrono::milliseconds guard = 15ms,
                                   cv::Rect roi = cv::Rect(0, 0, 4, 4))
{
    return scan::CameraRoiSyncTracker({roi, 40.0, 180.0, 3}, guard);
}

void confirmInitial(scan::CameraRoiSyncTracker& sync_tracker, MarkerState state, Clock::time_point start,
                    std::uint64_t first_sequence, int brightness)
{
    sync_tracker.beginPattern(state, start, 100ms);
    require(sync_tracker.observe(sample(first_sequence, start + 1ms, brightness)).status ==
                scan::CameraRoiSyncStatus::observing,
            "initial state confirmed too early");
    require(sync_tracker.observe(sample(first_sequence + 1, start + 2ms, brightness)).status ==
                scan::CameraRoiSyncStatus::observing,
            "initial state confirmed too early");
    require(sync_tracker.observe(sample(first_sequence + 2, start + 3ms, brightness)).status ==
                scan::CameraRoiSyncStatus::transition_confirmed,
            "initial state was not confirmed");
}

void testBlackToWhiteTransitionAndGuard()
{
    const auto t0 = Clock::time_point{1s};
    auto sync_tracker = tracker();
    confirmInitial(sync_tracker, MarkerState::black, t0 - 100ms, 1, 20);

    sync_tracker.beginPattern(MarkerState::white, t0, 100ms);
    require(sync_tracker.observe(sample(4, t0 - 10ms, 20)).status ==
                scan::CameraRoiSyncStatus::ignored_old_frame,
            "old black frame was not ignored");
    require(sync_tracker.observe(sample(5, t0 + 10ms, 20)).status == scan::CameraRoiSyncStatus::observing,
            "black frame unexpectedly confirmed white transition");
    require(sync_tracker.observe(sample(6, t0 + 20ms, 100)).marker_state == MarkerState::black,
            "hysteresis did not retain previous black state");
    require(sync_tracker.observe(sample(7, t0 + 30ms, 220)).status == scan::CameraRoiSyncStatus::observing,
            "white transition confirmed after one frame");
    require(sync_tracker.observe(sample(8, t0 + 40ms, 220)).status == scan::CameraRoiSyncStatus::observing,
            "white transition confirmed after two frames");
    const auto decision = sync_tracker.observe(sample(9, t0 + 50ms, 220));
    require(decision.status == scan::CameraRoiSyncStatus::transition_confirmed,
            "black to white transition was not confirmed after three frames");
    require(decision.transition_timestamp == t0 + 50ms, "transition timestamp is not the confirming frame time");
    require(decision.selected_at == t0 + 65ms, "guard was not added to the transition timestamp");
}

void testWhiteToBlackTransition()
{
    const auto t0 = Clock::time_point{2s};
    auto sync_tracker = tracker();
    confirmInitial(sync_tracker, MarkerState::white, t0 - 100ms, 1, 220);

    sync_tracker.beginPattern(MarkerState::black, t0, 100ms);
    require(sync_tracker.observe(sample(4, t0 + 10ms, 220)).status == scan::CameraRoiSyncStatus::observing,
            "white frame unexpectedly confirmed black transition");
    require(sync_tracker.observe(sample(5, t0 + 20ms, 20)).status == scan::CameraRoiSyncStatus::observing,
            "black transition confirmed after one frame");
    require(sync_tracker.observe(sample(6, t0 + 30ms, 20)).status == scan::CameraRoiSyncStatus::observing,
            "black transition confirmed after two frames");
    require(sync_tracker.observe(sample(7, t0 + 40ms, 20)).status ==
                scan::CameraRoiSyncStatus::transition_confirmed,
            "white to black transition was not confirmed after three frames");
}

void testHysteresisMiddleValue()
{
    const auto t0 = Clock::time_point{3s};
    auto sync_tracker = tracker();
    confirmInitial(sync_tracker, MarkerState::black, t0 - 100ms, 1, 20);
    sync_tracker.beginPattern(MarkerState::white, t0, 100ms);

    const auto middle = sync_tracker.observe(sample(4, t0 + 10ms, 100));
    require(middle.marker_state == MarkerState::black, "middle brightness did not preserve black state");
    require(!sync_tracker.confirmed(), "middle brightness caused a false transition");
}

void testOldFrameIgnored()
{
    const auto t0 = Clock::time_point{4s};
    auto sync_tracker = tracker();
    sync_tracker.beginPattern(MarkerState::white, t0, 100ms);
    require(sync_tracker.observe(sample(1, t0 - 1ms, 220)).status ==
                scan::CameraRoiSyncStatus::ignored_old_frame,
            "old frame was evaluated");
    sync_tracker.observe(sample(2, t0 + 1ms, 220));
    require(sync_tracker.observe(sample(3, t0 + 2ms, 220)).status == scan::CameraRoiSyncStatus::observing,
            "old frame increased the stable count");
    require(sync_tracker.observe(sample(4, t0 + 3ms, 220)).status ==
                scan::CameraRoiSyncStatus::transition_confirmed,
            "three eligible frames did not confirm transition");
}

void testDuplicateSequenceIgnoredAcrossPatternBoundary()
{
    const auto t0 = Clock::time_point{5s};
    auto sync_tracker = tracker();
    sync_tracker.beginPattern(MarkerState::white, t0, 100ms);
    const auto white = sample(1, t0 + 1ms, 220);
    require(sync_tracker.observe(white).status == scan::CameraRoiSyncStatus::observing,
            "first white frame was not observed");
    require(sync_tracker.observe(white).status == scan::CameraRoiSyncStatus::ignored_duplicate_frame,
            "duplicate frame was not ignored");
    require(sync_tracker.observe(white).status == scan::CameraRoiSyncStatus::ignored_duplicate_frame,
            "repeated duplicate frame was not ignored");
    require(sync_tracker.observe(sample(2, t0 + 2ms, 220)).status == scan::CameraRoiSyncStatus::observing,
            "duplicate frame increased the stable count");
    require(sync_tracker.observe(sample(3, t0 + 3ms, 220)).status ==
                scan::CameraRoiSyncStatus::transition_confirmed,
            "three unique frames did not confirm transition");

    sync_tracker.beginPattern(MarkerState::black, t0 + 10ms, 100ms);
    require(sync_tracker.observe(sample(3, t0 + 11ms, 20)).status ==
                scan::CameraRoiSyncStatus::ignored_duplicate_frame,
            "last evaluated sequence did not persist across patterns");
}

void testBrokenStabilityResets()
{
    const auto t0 = Clock::time_point{6s};
    auto sync_tracker = tracker();
    sync_tracker.beginPattern(MarkerState::white, t0, 100ms);
    sync_tracker.observe(sample(1, t0 + 1ms, 220));
    sync_tracker.observe(sample(2, t0 + 2ms, 220));
    require(sync_tracker.observe(sample(3, t0 + 3ms, 20)).status == scan::CameraRoiSyncStatus::observing,
            "opposite state unexpectedly confirmed transition");
    sync_tracker.observe(sample(4, t0 + 4ms, 220));
    require(sync_tracker.observe(sample(5, t0 + 5ms, 220)).status == scan::CameraRoiSyncStatus::observing,
            "stable count was not reset by opposite state");
    require(sync_tracker.observe(sample(6, t0 + 6ms, 220)).status ==
                scan::CameraRoiSyncStatus::transition_confirmed,
            "transition was not confirmed after the final three stable frames");
}

void testTimeoutWithoutSleeping()
{
    const auto t0 = Clock::time_point{7s};
    auto sync_tracker = tracker();
    sync_tracker.beginPattern(MarkerState::white, t0, 50ms);
    sync_tracker.observe(sample(1, t0 + 10ms, 20));
    require(!sync_tracker.timedOut(t0 + 49ms), "tracker timed out before deadline");
    require(sync_tracker.timedOut(t0 + 50ms), "tracker did not time out at deadline");
}

void testOutOfBoundsRoi()
{
    const auto t0 = Clock::time_point{8s};
    auto sync_tracker = tracker(15ms, cv::Rect(7, 7, 4, 4));
    sync_tracker.beginPattern(MarkerState::white, t0, 100ms);
    for (std::uint64_t sequence = 1; sequence <= 4; ++sequence)
    {
        const auto decision = sync_tracker.observe(sample(sequence, t0 + std::chrono::milliseconds(sequence), 220));
        require(decision.marker_state == MarkerState::undecided, "out-of-bounds ROI was not undecided");
        require(decision.status != scan::CameraRoiSyncStatus::transition_confirmed,
                "out-of-bounds ROI confirmed a transition");
    }
}

void testFirstPatternFromUndecided()
{
    const auto t0 = Clock::time_point{9s};
    auto sync_tracker = tracker();
    sync_tracker.beginPattern(MarkerState::black, t0, 100ms);
    sync_tracker.observe(sample(1, t0 + 1ms, 20));
    sync_tracker.observe(sample(2, t0 + 2ms, 20));
    require(sync_tracker.observe(sample(3, t0 + 3ms, 20)).status ==
                scan::CameraRoiSyncStatus::transition_confirmed,
            "first pattern did not confirm from undecided state");
}

} // namespace

int main()
{
    testBlackToWhiteTransitionAndGuard();
    testWhiteToBlackTransition();
    testHysteresisMiddleValue();
    testOldFrameIgnored();
    testDuplicateSequenceIgnoredAcrossPatternBoundary();
    testBrokenStabilityResets();
    testTimeoutWithoutSleeping();
    testOutOfBoundsRoi();
    testFirstPatternFromUndecided();
    return 0;
}
