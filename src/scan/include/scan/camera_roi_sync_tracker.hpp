#pragma once

#include "structured_light/pattern_sync.hpp"
#include "video/video_types.hpp"

#include <chrono>
#include <cstdint>
#include <optional>

namespace scan
{

enum class CameraRoiSyncStatus
{
    ignored_old_frame,
    ignored_duplicate_frame,
    observing,
    transition_confirmed,
};

struct CameraRoiSyncDecision
{
    CameraRoiSyncStatus status{CameraRoiSyncStatus::observing};
    structured_light::sync::MarkerState marker_state{structured_light::sync::MarkerState::undecided};
    std::optional<std::chrono::steady_clock::time_point> transition_timestamp;
    std::optional<std::chrono::steady_clock::time_point> selected_at;
};

/**
 * @brief scan中のcamera ROI pattern transitionをdeterministicに追跡する。
 *
 * previous marker stateと最後に評価したframe sequenceはpattern境界を跨いで保持する。
 * clock取得、sleep、Camera/Projector I/Oはcallerの責務である。
 */
class CameraRoiSyncTracker
{
  public:
    CameraRoiSyncTracker(structured_light::sync::RoiSyncConfig config, std::chrono::milliseconds guard);

    void beginPattern(structured_light::sync::MarkerState expected,
                      std::chrono::steady_clock::time_point show_timestamp,
                      std::chrono::milliseconds timeout);

    CameraRoiSyncDecision observe(const video::FrameSample& sample);

    bool timedOut(std::chrono::steady_clock::time_point now) const;
    bool confirmed() const noexcept;

  private:
    structured_light::sync::RoiSyncConfig config_;
    std::chrono::milliseconds guard_;

    structured_light::sync::MarkerState previous_stable_state_{
        structured_light::sync::MarkerState::undecided};
    std::uint64_t last_evaluated_sequence_{0};

    structured_light::sync::MarkerState expected_{structured_light::sync::MarkerState::undecided};
    std::chrono::steady_clock::time_point show_timestamp_{};
    std::chrono::steady_clock::time_point deadline_{};
    int stable_count_{0};
    bool saw_transition_candidate_{false};
    bool confirmed_{false};
};

} // namespace scan
