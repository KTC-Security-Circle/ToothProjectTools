#include "structured_light/pattern_sync.hpp"

namespace structured_light::sync
{
std::string toString(MarkerState state)
{
    switch (state) { case MarkerState::black: return "black"; case MarkerState::white: return "white"; case MarkerState::undecided: return "undecided"; }
    return "undecided";
}
std::string toString(SyncSource source)
{
    switch (source) { case SyncSource::photodiode: return "photodiode"; }
    return "unknown";
}

std::chrono::steady_clock::time_point selectionTime(const SyncEvent& event, std::chrono::milliseconds guard)
{
    return event.timestamp + guard;
}

std::optional<SyncEvent> PhotodiodeSyncSource::waitForTransition(
    MarkerState expected, std::chrono::steady_clock::time_point after,
    std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        const auto event = transport_.receive(remaining);
        if (!event)
            return std::nullopt;
        if (event->timestamp >= after && event->state == expected)
        {
            auto accepted = *event;
            accepted.source = SyncSource::photodiode;
            return accepted;
        }
    }
    return std::nullopt;
}
} // namespace structured_light::sync
