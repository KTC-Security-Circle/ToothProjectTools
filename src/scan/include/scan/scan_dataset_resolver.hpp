#pragma once

#include "scan/scan_dataset_result.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace service::scan_dataset
{

struct ScanDatasetInputSpec
{
    std::optional<std::filesystem::path> input_dir;
    std::optional<std::filesystem::path> left_dir;
    std::optional<std::filesystem::path> right_dir;
    std::optional<std::filesystem::path> metadata_file;
    bool allow_partial{false};
    std::optional<int> projector_width;
    std::optional<int> projector_height;
    std::optional<int> pattern_count;
};

struct ResolvedScanDataset
{
    std::filesystem::path root_dir;
    std::filesystem::path left_dir;
    std::filesystem::path right_dir;
    std::optional<std::filesystem::path> metadata_file;
    bool metadata_present{false};
    int pattern_count{0};
    int width{0};
    int height{0};
};

struct ScanDatasetResolveResult
{
    bool ok{false};
    ResolvedScanDataset dataset;
    std::vector<ScanDatasetIssue> issues;
};

class ScanDatasetResolver
{
  public:
    ScanDatasetResolveResult resolve(const ScanDatasetInputSpec& spec) const;
};

int inferPatternCount(const std::filesystem::path& left_dir, const std::filesystem::path& right_dir);

} // namespace service::scan_dataset
