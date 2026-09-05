#pragma once

#include <opencv2/core.hpp>

#include <cstddef>
#include <optional>
#include <vector>

namespace calib
{

/**
 * @brief 画像上で観測したcheckerboardの幾何情報を表す。
 *
 * cornerは左上原点のcamera pixel座標で、boardのrow-major順に格納する。
 * 距離の代わりに画像占有率を使うため、Calibration前でも近・中・遠を判定できる。
 */
struct BoardObservation
{
    bool detected{false};
    std::vector<cv::Point2f> corners;
    cv::Point2f center{};
    cv::Size2f size{};
    float rotation_deg{0.0F};
    float perspective_error{0.0F};
    float blur_score{0.0F};
};

enum class BoardRange
{
    unknown,
    near,
    middle,
    far,
};

struct GuidanceThresholds
{
    float center_tolerance_ratio{0.12F};
    float min_area_ratio{0.08F};
    float max_area_ratio{0.55F};
    float rotation_tolerance_deg{8.0F};
    float perspective_tolerance{0.20F};
    float minimum_blur_score{40.0F};
    float stability_mean_px{0.35F};
    float stability_max_px{1.25F};
    std::size_t stable_frames{5};
};

struct BoardTarget
{
    cv::Point2f center_ratio{0.5F, 0.5F};
    float area_ratio{0.25F};
    float rotation_deg{0.0F};
    BoardRange range{BoardRange::middle};
};

struct StabilityResult
{
    bool stable{false};
    float mean_motion_px{0.0F};
    float max_motion_px{0.0F};
    std::size_t consecutive_frames{0};
};

/** @brief 入力画像からcheckerboardを検出し、cornerをsub-pixel refinementする。 */
std::optional<BoardObservation> observeBoard(const cv::Mat& image, const cv::Size& pattern_size);

/** @brief 観測が指定targetと撮影成立条件を満たすか判定する。 */
bool meetsTarget(const BoardObservation& observation,
                 const BoardTarget& target,
                 const cv::Size& image_size,
                 const GuidanceThresholds& thresholds);

/** @brief 前回観測との差分から移動量と連続安定frame数を更新する。 */
StabilityResult evaluateStability(const std::vector<cv::Point2f>& previous,
                                  const std::vector<cv::Point2f>& current,
                                  std::size_t previous_consecutive_frames,
                                  const GuidanceThresholds& thresholds);

/** @brief board面積の画像占有率からCalibration前の距離区分を求める。 */
BoardRange classifyRange(float area_ratio, const GuidanceThresholds& thresholds);

/** @brief checkerboard検出結果を画像へ描画する。 */
void drawBoardObservation(cv::Mat& image, const BoardObservation& observation, const cv::Size& pattern_size);

} // namespace calib
