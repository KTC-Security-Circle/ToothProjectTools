#include "scan/serial_photodiode_transport.hpp"
#include "scan/scan_save_queue.hpp"
#include "scan/sync_delay.hpp"
#include "structured_light/pattern_sync.hpp"
#include "video/video_types.hpp"

#include <chrono>
#include <algorithm>
#include <deque>
#include <opencv2/core.hpp>
#include <stdexcept>
#include <string>
#include <cmath>
#include <atomic>
#include <condition_variable>
#include <future>
#include <mutex>
#include <vector>

namespace
{
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;
using structured_light::sync::MarkerState;
using structured_light::sync::SyncEvent;

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

class FakeTransport final : public structured_light::sync::PhotodiodeTransport
{
  public:
    explicit FakeTransport(std::deque<std::optional<SyncEvent>> events) : events_(std::move(events)) {}
    std::optional<SyncEvent> receive(std::chrono::milliseconds) override
    {
        if (events_.empty()) return std::nullopt;
        auto event = events_.front();
        events_.pop_front();
        return event;
    }
  private:
    std::deque<std::optional<SyncEvent>> events_;
};

SyncEvent event(MarkerState state, Clock::time_point timestamp, std::uint64_t sequence)
{
    return {state, timestamp, sequence, structured_light::sync::SyncSource::photodiode, 1.0};
}

video::FrameSample frame(std::uint64_t sequence, Clock::time_point timestamp)
{
    return {cv::Mat(2, 2, CV_8UC1, cv::Scalar(1)), sequence, timestamp};
}

capture::CaptureResult savedResult();

void testProtocol()
{
    const auto t = Clock::time_point{1s};
    require(scan::parsePhotodiodeLine("0", t, 1).state == MarkerState::black, "0 was not black");
    require(scan::parsePhotodiodeLine("1", t, 2).state == MarkerState::white, "1 was not white");
    bool invalid = false;
    try { (void)scan::parsePhotodiodeLine("invalid", t, 3); }
    catch (const scan::PhotodiodeTransportError& error)
    {
        invalid = error.code() == "photodiode_invalid_event";
    }
    require(invalid, "invalid protocol line was accepted");
    require(scan::parsePhotodiodeLine("0", t, 4).state == MarkerState::black,
            "parser did not recover for a subsequent black event");
}

void testExpectedStateAndOldEvent()
{
    const auto show = Clock::now() - 10ms;
    FakeTransport transport({event(MarkerState::white, show - 1ms, 1),
                             event(MarkerState::black, show + 1ms, 2),
                             event(MarkerState::white, show + 2ms, 3)});
    structured_light::sync::PhotodiodeSyncSource source(transport);
    const auto accepted = source.waitForTransition(MarkerState::white, show, 100ms);
    require(accepted && accepted->sequence == 3, "old or wrong-state event was accepted");
}

void testTimeout()
{
    FakeTransport transport({std::nullopt});
    structured_light::sync::PhotodiodeSyncSource source(transport);
    require(!source.waitForTransition(MarkerState::white, Clock::now(), 1ms),
            "missing event did not produce photodiode timeout condition");
}

void testGuardAndFrameSelection()
{
    const auto t = Clock::time_point{2s};
    const auto pd = event(MarkerState::white, t, 1);
    const auto selected_at = structured_light::sync::selectionTime(pd, 30ms);
    require(selected_at == t + 30ms, "guard calculation is incorrect");

    video::FrameRingBuffer frames{frame(1, t + 10ms), frame(2, t + 20ms),
                                  frame(3, t + 35ms), frame(4, t + 50ms)};
    const auto selected = video::firstFrameAtOrAfter(frames, selected_at);
    require(selected && selected->sequence == 3, "first frame at or after guard was not selected");

    // Selection API owns exactly one deep copy; later ring-buffer writes cannot mutate it.
    frames[2].image.setTo(cv::Scalar(99));
    require(selected->image.at<unsigned char>(0, 0) == 1,
            "selected frame still aliases the camera ring buffer");
}

void testMovedSaveJobRetainsIndependentFrame()
{
    const auto timestamp = Clock::now();
    video::FrameRingBuffer ring{frame(1, timestamp)};
    auto selected = video::firstFrameAtOrAfter(ring, timestamp);
    require(selected.has_value(), "frame selection failed");
    std::atomic<int> saved_pixel{-1};
    scan::ScanSaveQueue queue(1, 2, [&](const video::FrameSample& sample, const std::filesystem::path&) {
        saved_pixel = sample.image.at<unsigned char>(0, 0);
        return savedResult();
    });
    require(queue.enqueue({"scan_test", 0, "graycode", "black", "left",
                           std::move(*selected), "unused.png"}), "save enqueue failed");
    ring.front().image.setTo(cv::Scalar(77));
    queue.closeAndWait();
    require(saved_pixel.load() == 1, "queued save frame changed when camera ring advanced");
}

void testPreArmAndPatternParitySequence()
{
    const auto t = Clock::time_point{4s};
    FakeTransport transport({event(MarkerState::white, t + 1ms, 1),
                             event(MarkerState::black, t + 2ms, 2),
                             event(MarkerState::white, t + 3ms, 3)});
    structured_light::sync::PhotodiodeSyncSource source(transport);
    require(source.waitForTransition(MarkerState::white, t, 10ms).has_value(), "white pre-arm failed");
    require(source.waitForTransition(MarkerState::black, t, 10ms).has_value(), "pattern 0 black failed");
    require(source.waitForTransition(MarkerState::white, t, 10ms).has_value(), "pattern 1 white failed");
}

void testStereoUsesSameSelectionTimestamp()
{
    const auto t = Clock::time_point{3s};
    const auto selected_at = t + 30ms;
    video::FrameRingBuffer left{frame(10, t + 20ms), frame(11, t + 35ms)};
    video::FrameRingBuffer right{frame(20, t + 25ms), frame(21, t + 40ms)};
    const auto selected_left = video::firstFrameAtOrAfter(left, selected_at);
    const auto selected_right = video::firstFrameAtOrAfter(right, selected_at);
    require(selected_left && selected_left->sequence == 11, "left camera selection failed");
    require(selected_right && selected_right->sequence == 21, "right camera selection failed");
}

void testAdaptiveMeasurementModelAndClassification()
{
    cv::Mat black(10, 10, CV_8UC1, cv::Scalar(40));
    cv::Mat white(10, 10, CV_8UC1, cv::Scalar(210));
    // 反転応答pixelも符号付きbaselineで正しく分類する。
    black(cv::Rect(0, 0, 5, 10)).setTo(220);
    white(cv::Rect(0, 0, 5, 10)).setTo(30);
    const auto model = scan::buildMeasurementModel(black, white, 30.0);
    require(model && model->pixel_count == 100, "baseline mask was not generated");
    double ratio = 0.0;
    require(scan::frameMatches(*model, white, MarkerState::white, 0.90, &ratio) && ratio == 1.0,
            "white classification failed");
    require(scan::frameMatches(*model, black, MarkerState::black, 0.90, &ratio) && ratio == 1.0,
            "black classification failed");
    cv::Mat partial = white.clone();
    black(cv::Rect(0, 0, 2, 10)).copyTo(partial(cv::Rect(0, 0, 2, 10)));
    require(!scan::frameMatches(*model, partial, MarkerState::white, 0.90, &ratio) && ratio < 0.90,
            "required_ratio was not enforced");
}

void testInsufficientContrast()
{
    cv::Mat black(10, 10, CV_8UC1, cv::Scalar(100));
    cv::Mat white(10, 10, CV_8UC1, cv::Scalar(110));
    require(!scan::buildMeasurementModel(black, white, 30.0), "insufficient contrast was accepted");
}

void testDelayStatisticsAndAlternation()
{
    const auto stats = scan::calculateSyncDelayStatistics({10, 20, 30, 40, 50}, 5.0);
    require(stats.count == 5 && stats.mean_ms == 30.0 && stats.median_ms == 30.0,
            "mean or median is incorrect");
    require(std::abs(stats.p95_ms - 48.0) < 0.001 && std::abs(stats.p99_ms - 49.6) < 0.001,
            "percentiles are incorrect");
    require(stats.max_ms == 50.0 && stats.recommended_guard_ms == 55,
            "max or recommended guard is incorrect");
    const auto states = scan::measurementStateSequence(60);
    require(states.size() == 60, "transition count is incorrect");
    for (std::size_t index = 0; index < states.size(); ++index)
        require(states[index] == (index % 2 == 0 ? MarkerState::white : MarkerState::black),
                "measurement states do not alternate");
}

void testPhotodiodeToCameraTransitionDelay()
{
    const auto timestamp = Clock::time_point{5s};
    cv::Mat black(8, 8, CV_8UC1, cv::Scalar(0));
    cv::Mat white(8, 8, CV_8UC1, cv::Scalar(255));
    const auto model = scan::buildMeasurementModel(black, white, 30.0);
    require(model.has_value(), "delay model creation failed");
    const std::vector<video::FrameSample> frames{
        {black, 1, timestamp + 5ms}, {black, 2, timestamp + 15ms}, {white, 3, timestamp + 25ms}};
    const auto found = std::find_if(frames.begin(), frames.end(), [&](const auto& sample) {
        return sample.timestamp >= timestamp && scan::frameMatches(*model, sample.image, MarkerState::white, 0.9);
    });
    require(found != frames.end() && found->sequence == 3, "first transitioned frame was not selected");
    require(std::chrono::duration_cast<std::chrono::milliseconds>(found->timestamp - timestamp).count() == 25,
            "Photodiode-to-Camera delay is incorrect");
}

capture::CaptureResult savedResult()
{
    capture::CaptureResult result;
    result.ok = true;
    return result;
}

scan::ScanSaveJob saveJob(int pattern, std::string side)
{
    return {"scan_test", pattern, "graycode", "white", std::move(side),
            frame(static_cast<std::uint64_t>(pattern + 1), Clock::now()),
            std::filesystem::path("unused.png")};
}

void testSaveQueuePreSaveEventAndParallelism()
{
    std::mutex mutex;
    std::condition_variable condition;
    bool release = false;
    std::atomic<int> active{0};
    std::atomic<int> maximum{0};
    std::vector<std::string> events{"scan_pattern_shown", "scan_frame_selected"};
    scan::ScanSaveQueue queue(2, 8,
        [&](const video::FrameSample&, const std::filesystem::path&) {
            const int current = ++active;
            maximum.store(std::max(maximum.load(), current));
            std::unique_lock lock(mutex);
            condition.wait(lock, [&] { return release; });
            --active;
            return savedResult();
        },
        [&](const scan::ScanSaveJob&) {
            std::lock_guard lock(mutex);
            events.push_back("scan_frame_captured");
        });
    require(queue.enqueue(saveJob(0, "left")), "left save enqueue failed");
    require(queue.enqueue(saveJob(0, "right")), "right save enqueue failed");
    for (int attempt = 0; attempt < 100 && active.load() < 2; ++attempt) std::this_thread::sleep_for(1ms);
    require(active.load() == 2 && maximum.load() == 2, "two save workers did not run concurrently");
    require(events.size() == 2, "save completion was emitted before blocked saves completed");
    {
        std::lock_guard lock(mutex);
        release = true;
    }
    condition.notify_all();
    queue.closeAndWait();
    require(events.size() == 4, "save completion callbacks were not emitted");
    require(events[0] == "scan_pattern_shown" && events[1] == "scan_frame_selected",
            "selected event was not emitted before save completion");
}

void testSaveQueueDoesNotSerializeProducerBehindSave()
{
    std::mutex mutex;
    std::condition_variable condition;
    bool release = false;
    scan::ScanSaveQueue queue(1, 8, [&](const video::FrameSample&, const std::filesystem::path&) {
        std::unique_lock lock(mutex);
        condition.wait_for(lock, 100ms, [&] { return release; });
        return savedResult();
    });
    const auto begin = Clock::now();
    require(queue.enqueue(saveJob(0, "left")), "first asynchronous enqueue failed");
    require(queue.enqueue(saveJob(1, "left")), "next-pattern enqueue failed");
    require(Clock::now() - begin < 50ms, "next pattern was serialized behind the 100ms save worker");
    {
        std::lock_guard lock(mutex);
        release = true;
    }
    condition.notify_all();
    queue.closeAndWait();
}

void testSaveQueueIsBoundedAndWaitsForDrain()
{
    std::mutex mutex;
    std::condition_variable condition;
    bool release = false;
    std::atomic<int> active{0};
    scan::ScanSaveQueue queue(2, 2, [&](const video::FrameSample&, const std::filesystem::path&) {
        ++active;
        std::unique_lock lock(mutex);
        condition.wait(lock, [&] { return release; });
        --active;
        return savedResult();
    });
    for (int index = 0; index < 2; ++index) require(queue.enqueue(saveJob(index, "left")), "enqueue failed");
    for (int attempt = 0; attempt < 100 && active.load() < 2; ++attempt) std::this_thread::sleep_for(1ms);
    require(active.load() == 2, "workers did not start before bounded queue test");
    for (int index = 2; index < 4; ++index) require(queue.enqueue(saveJob(index, "left")), "enqueue failed");
    auto blocked_enqueue = std::async(std::launch::async, [&] { return queue.enqueueWithTiming(saveJob(4, "left")); });
    require(blocked_enqueue.wait_for(30ms) == std::future_status::timeout,
            "producer did not apply backpressure at queue capacity");
    {
        std::lock_guard lock(mutex);
        release = true;
    }
    condition.notify_all();
    const auto enqueue_timing = blocked_enqueue.get();
    require(enqueue_timing.accepted && enqueue_timing.wait_ms >= 20.0,
            "blocked enqueue did not report its backpressure wait");
    queue.closeAndWait();

    bool drain_release = false;
    scan::ScanSaveQueue drain_queue(1, 1, [&](const video::FrameSample&, const std::filesystem::path&) {
        std::unique_lock lock(mutex);
        condition.wait(lock, [&] { return drain_release; });
        return savedResult();
    });
    require(drain_queue.enqueue(saveJob(9, "left")), "drain job enqueue failed");
    auto drain = std::async(std::launch::async, [&] { drain_queue.closeAndWait(); });
    require(drain.wait_for(30ms) == std::future_status::timeout,
            "queue reported completion while a save was still running");
    {
        std::lock_guard lock(mutex);
        drain_release = true;
    }
    condition.notify_all();
    drain.get();
}

void testSaveQueueReportsFailure()
{
    scan::ScanSaveQueue queue(2, 8, [](const video::FrameSample&, const std::filesystem::path&) {
        capture::CaptureResult result;
        result.error = capture::CaptureError{capture::CaptureErrorCode::FileWriteFailed, "intentional failure"};
        return result;
    });
    require(queue.enqueue(saveJob(7, "right")), "failure job enqueue failed");
    queue.closeAndWait();
    const auto failure = queue.failure();
    require(failure && failure->pattern_index == 7 && failure->side == "right" &&
            failure->error == "intentional failure", "save failure details were lost");
}

} // namespace

int main()
{
    testProtocol();
    testExpectedStateAndOldEvent();
    testTimeout();
    testGuardAndFrameSelection();
    testMovedSaveJobRetainsIndependentFrame();
    testStereoUsesSameSelectionTimestamp();
    testPreArmAndPatternParitySequence();
    testAdaptiveMeasurementModelAndClassification();
    testInsufficientContrast();
    testDelayStatisticsAndAlternation();
    testPhotodiodeToCameraTransitionDelay();
    testSaveQueuePreSaveEventAndParallelism();
    testSaveQueueDoesNotSerializeProducerBehindSave();
    testSaveQueueIsBoundedAndWaitsForDrain();
    testSaveQueueReportsFailure();
    return 0;
}
