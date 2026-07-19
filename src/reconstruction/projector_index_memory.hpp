#pragma once

#include <cstddef>
#include <limits>
#include <optional>

#include <opencv2/core.hpp>

namespace reconstruction::detail
{
constexpr std::size_t kProjectorIndexMemoryWarningBytes = 256ULL * 1024ULL * 1024ULL;
constexpr std::size_t kProjectorIndexMemoryHardLimitBytes = 1024ULL * 1024ULL * 1024ULL;

struct ProjectorIndexMemoryEstimate
{
    std::size_t bucket_count{};
    std::size_t offsets_bytes{};
    std::size_t cursors_bytes{};
    std::size_t candidates_bytes{};
    std::size_t estimated_peak_bytes{};
};

inline bool checkedMultiply(std::size_t left, std::size_t right, std::size_t& output)
{
    if (right != 0 && left > std::numeric_limits<std::size_t>::max() / right)
    {
        return false;
    }
    output = left * right;
    return true;
}

inline bool checkedAdd(std::size_t left, std::size_t right, std::size_t& output)
{
    if (left > std::numeric_limits<std::size_t>::max() - right)
    {
        return false;
    }
    output = left + right;
    return true;
}

inline std::optional<ProjectorIndexMemoryEstimate>
estimateProjectorIndexMemory(int projector_width,
                             int projector_height,
                             std::size_t maximum_candidate_count)
{
    if (projector_width <= 0 || projector_height <= 0)
    {
        return std::nullopt;
    }

    ProjectorIndexMemoryEstimate estimate;
    const auto width = static_cast<std::size_t>(projector_width);
    const auto height = static_cast<std::size_t>(projector_height);
    if (!checkedMultiply(width, height, estimate.bucket_count))
    {
        return std::nullopt;
    }

    std::size_t offset_count = 0;
    if (!checkedAdd(estimate.bucket_count, 1, offset_count) ||
        !checkedMultiply(offset_count, sizeof(std::size_t), estimate.offsets_bytes) ||
        !checkedMultiply(estimate.bucket_count, sizeof(std::size_t), estimate.cursors_bytes) ||
        !checkedMultiply(maximum_candidate_count,
                         sizeof(cv::Point2f),
                         estimate.candidates_bytes))
    {
        return std::nullopt;
    }

    std::size_t offsets_and_cursors = 0;
    if (!checkedAdd(estimate.offsets_bytes, estimate.cursors_bytes, offsets_and_cursors) ||
        !checkedAdd(offsets_and_cursors,
                    estimate.candidates_bytes,
                    estimate.estimated_peak_bytes))
    {
        return std::nullopt;
    }
    return estimate;
}

inline bool isProjectorIndexMemoryWarningLevel(const ProjectorIndexMemoryEstimate& estimate)
{
    return estimate.estimated_peak_bytes >= kProjectorIndexMemoryWarningBytes &&
           estimate.estimated_peak_bytes < kProjectorIndexMemoryHardLimitBytes;
}

inline bool isProjectorIndexMemoryHardLimitExceeded(const ProjectorIndexMemoryEstimate& estimate)
{
    return estimate.estimated_peak_bytes >= kProjectorIndexMemoryHardLimitBytes;
}
} // namespace reconstruction::detail
