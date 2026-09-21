#include "scan/serial_photodiode_transport.hpp"
#include "structured_light/pattern_sync.hpp"
#include "video/video_types.hpp"

#include <chrono>
#include <deque>
#include <opencv2/core.hpp>
#include <stdexcept>
#include <string>

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

    video::FrameRingBuffer frames{frame(1, t - 5ms), frame(2, t + 5ms),
                                  frame(3, t + 20ms), frame(4, t + 40ms)};
    const auto selected = video::firstFrameAtOrAfter(frames, selected_at);
    require(selected && selected->sequence == 4, "first frame at or after guard was not selected");
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

} // namespace

int main()
{
    testProtocol();
    testExpectedStateAndOldEvent();
    testTimeout();
    testGuardAndFrameSelection();
    testStereoUsesSameSelectionTimestamp();
    return 0;
}
