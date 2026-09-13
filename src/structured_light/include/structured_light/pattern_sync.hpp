#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <optional>

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

namespace structured_light::sync
{
enum class MarkerState { black, white, undecided };
enum class SyncSource { camera_roi, photodiode, fixed_delay };

/**
 * @brief 同期markerの光学状態とhost monotonic timestampを表す。
 *
 * Photodiode eventもdevice clockではなくhost受信時のsteady_clockを使い、
 * Camera frameと同じclock domainで比較する。
 */
struct SyncEvent
{
    MarkerState state{MarkerState::undecided};
    std::chrono::steady_clock::time_point timestamp{};
    std::uint64_t sequence{0};
    SyncSource source{SyncSource::camera_roi};
    double confidence{0.0};
};

struct RoiSyncConfig
{
    cv::Rect roi;
    double black_threshold{40.0};
    double white_threshold{180.0};
    int stable_frames{3};
};

struct RoiObservation { MarkerState state{MarkerState::undecided}; double mean_brightness{0.0}; };

/** @brief 光学同期イベントを供給する抽象境界。timestampはhostのsteady_clock domainである。 */
class PatternSyncSource
{
  public:
    virtual ~PatternSyncSource() = default;
    virtual std::optional<SyncEvent> waitForTransition(
        MarkerState expected, std::chrono::steady_clock::time_point after,
        std::chrono::milliseconds timeout) = 0;
};

/** @brief Photodiodeのdevice transportを同期判定から分離する受信境界。 */
class PhotodiodeTransport
{
  public:
    virtual ~PhotodiodeTransport() = default;
    virtual std::optional<SyncEvent> receive(std::chrono::milliseconds timeout) = 0;
};

/** @brief Photodiode transportのeventをCameraと同じ同期契約へ適合する。 */
class PhotodiodeSyncSource final : public PatternSyncSource
{
  public:
    explicit PhotodiodeSyncSource(PhotodiodeTransport& transport) : transport_(transport) {}
    std::optional<SyncEvent> waitForTransition(
        MarkerState expected, std::chrono::steady_clock::time_point after,
        std::chrono::milliseconds timeout) override;

  private:
    PhotodiodeTransport& transport_;
};

/** @brief ROI平均輝度をhysteresis thresholdでmarker状態へ変換する。 */
RoiObservation observeRoi(const cv::Mat&, const RoiSyncConfig&, MarkerState previous_state);
std::string toString(MarkerState);
std::string toString(SyncSource);
} // namespace structured_light::sync
