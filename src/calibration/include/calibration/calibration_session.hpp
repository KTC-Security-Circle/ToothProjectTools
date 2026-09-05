#pragma once

#include "calibration/calibration_observation.hpp"

#include <cstddef>

namespace calib
{

enum class SessionState
{
    waiting_for_board,
    guiding,
    stabilizing,
    ready_to_capture,
    captured,
    next_pose,
    solving,
    validating,
    completed,
    failed,
    cancelled,
};

enum class CaptureMode
{
    automatic,
    manual,
};

/**
 * @brief Calibration撮影の状態遷移とcapture成立条件を保持する。
 *
 * Camera、Window、filesystemへアクセスせず、1 frame分の観測だけを入力に取る。
 * 実際のcaptureやsolver呼び出しは上位workflowがこの値を見て実行する。
 */
class CalibrationSession final
{
public:
    explicit CalibrationSession(CaptureMode mode = CaptureMode::automatic) noexcept : mode_(mode) {}

    SessionState state() const noexcept { return state_; }
    CaptureMode mode() const noexcept { return mode_; }
    std::size_t captured_count() const noexcept { return captured_count_; }
    std::size_t stable_frames() const noexcept { return stable_frames_; }

    void begin() noexcept { state_ = SessionState::waiting_for_board; }
    void fail() noexcept { state_ = SessionState::failed; }
    void cancel() noexcept { state_ = SessionState::cancelled; }
    void beginSolving() noexcept { state_ = SessionState::solving; }
    void beginValidation() noexcept { state_ = SessionState::validating; }
    void complete() noexcept { state_ = SessionState::completed; }

    /** @brief frame観測を評価し、撮影可能状態までのみ遷移させる。 */
    void observe(const BoardObservation& observation,
                 const BoardTarget& target,
                 const cv::Size& image_size,
                 const StabilityResult& stability,
                 const GuidanceThresholds& thresholds)
    {
        if (state_ == SessionState::failed || state_ == SessionState::cancelled ||
            state_ == SessionState::completed || state_ == SessionState::solving ||
            state_ == SessionState::validating)
        {
            return;
        }
        if (!observation.detected)
        {
            stable_frames_ = 0;
            state_ = SessionState::waiting_for_board;
            return;
        }
        if (!meetsTarget(observation, target, image_size, thresholds))
        {
            stable_frames_ = 0;
            state_ = SessionState::guiding;
            return;
        }
        stable_frames_ = stability.consecutive_frames;
        state_ = stability.stable ? SessionState::ready_to_capture : SessionState::stabilizing;
    }

    /** @brief 撮影成功後に次のposeへ進める。 */
    void recordCapture() noexcept
    {
        if (state_ != SessionState::ready_to_capture)
        {
            return;
        }
        ++captured_count_;
        state_ = SessionState::captured;
    }

    void advanceToNextPose() noexcept
    {
        if (state_ == SessionState::captured)
        {
            state_ = SessionState::next_pose;
            stable_frames_ = 0;
        }
    }

private:
    CaptureMode mode_;
    SessionState state_{SessionState::waiting_for_board};
    std::size_t captured_count_{0};
    std::size_t stable_frames_{0};
};

} // namespace calib
