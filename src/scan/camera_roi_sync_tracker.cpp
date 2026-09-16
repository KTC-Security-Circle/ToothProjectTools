#include "scan/camera_roi_sync_tracker.hpp"

#include <utility>

namespace scan
{

CameraRoiSyncTracker::CameraRoiSyncTracker(structured_light::sync::RoiSyncConfig config,
                                           std::chrono::milliseconds guard)
    : config_(std::move(config)), guard_(guard)
{
}

void CameraRoiSyncTracker::beginPattern(structured_light::sync::MarkerState expected,
                                        std::chrono::steady_clock::time_point show_timestamp,
                                        std::chrono::milliseconds timeout)
{
    expected_ = expected;
    show_timestamp_ = show_timestamp;
    deadline_ = show_timestamp + timeout;
    stable_count_ = 0;
    saw_transition_candidate_ = previous_stable_state_ == structured_light::sync::MarkerState::undecided;
    confirmed_ = false;
}

CameraRoiSyncDecision CameraRoiSyncTracker::observe(const video::FrameSample& sample)
{
    if (sample.timestamp < show_timestamp_)
    {
        return {CameraRoiSyncStatus::ignored_old_frame, structured_light::sync::MarkerState::undecided,
                std::nullopt, std::nullopt};
    }
    if (sample.sequence == last_evaluated_sequence_)
    {
        return {CameraRoiSyncStatus::ignored_duplicate_frame, structured_light::sync::MarkerState::undecided,
                std::nullopt, std::nullopt};
    }

    last_evaluated_sequence_ = sample.sequence;
    const auto observation = structured_light::sync::observeRoi(sample.image, config_, previous_stable_state_);
    if (previous_stable_state_ != structured_light::sync::MarkerState::undecided &&
        observation.state != previous_stable_state_)
    {
        saw_transition_candidate_ = true;
    }
    if (observation.state != expected_ && observation.state != structured_light::sync::MarkerState::undecided)
    {
        saw_transition_candidate_ = true;
        stable_count_ = 0;
    }
    else if (observation.state == expected_ && saw_transition_candidate_)
    {
        ++stable_count_;
    }
    else
    {
        stable_count_ = 0;
    }

    if (saw_transition_candidate_ && stable_count_ >= config_.stable_frames)
    {
        previous_stable_state_ = expected_;
        confirmed_ = true;
        const auto selected_at = sample.timestamp + guard_;
        return {CameraRoiSyncStatus::transition_confirmed, observation.state, sample.timestamp, selected_at};
    }
    return {CameraRoiSyncStatus::observing, observation.state, std::nullopt, std::nullopt};
}

bool CameraRoiSyncTracker::timedOut(std::chrono::steady_clock::time_point now) const
{
    return !confirmed_ && now >= deadline_;
}

bool CameraRoiSyncTracker::confirmed() const noexcept
{
    return confirmed_;
}

} // namespace scan
