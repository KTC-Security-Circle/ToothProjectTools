#pragma once

#include "scan/scan_dataset_result.hpp"
#include "scan/scan_dataset_resolver.hpp"

#include <filesystem>
#include <optional>
#include <vector>

namespace scan::dataset
{

struct ScanDatasetValidationConfig : ScanDatasetInputSpec
{
    ScanDatasetValidationConfig() = default;
    ScanDatasetValidationConfig(std::filesystem::path input, bool allow)
    {
        input_dir = std::move(input);
        allow_partial = allow;
    }
};

class ScanDatasetValidator
{
  public:
    /// @brief scan datasetを検証する。
    ///
    /// Args:
    ///   config <const ScanDatasetValidationConfig&>: validation設定。
    ///
    /// Return:
    ///   <ScanDatasetValidationResult>: validation結果。
    ScanDatasetValidationResult validate(const ScanDatasetValidationConfig& config) const;

    /// @brief decode処理向けにmetadata.jsonを読み取る。
    ///
    /// Args:
    ///   input_dir <const std::filesystem::path&>: scan dataset directory。
    ///   issues <std::vector<ScanDatasetIssue>&>: 検出issueの追加先。
    ///
    /// Return:
    ///   <std::optional<ScanDatasetMetadata>>: parse可能なmetadata。致命的なparse失敗時はstd::nullopt。
    std::optional<ScanDatasetMetadata> readMetadataForDecode(
        const std::filesystem::path& input_dir,
        std::vector<ScanDatasetIssue>& issues) const;

    /// @brief decode処理向けに指定metadata fileを読み取る。
    std::optional<ScanDatasetMetadata> readMetadataFileForDecode(
        const std::filesystem::path& metadata_file,
        std::vector<ScanDatasetIssue>& issues) const;

  private:
    /// @brief metadata.jsonを読み取り、固定schemaから必要fieldを取り出す。
    ///
    /// Args:
    ///   metadata_path <const std::filesystem::path&>: 読み取り対象metadata path。
    ///   issues <std::vector<ScanDatasetIssue>&>: 検出issueの追加先。
    ///
    /// Return:
    ///   <std::optional<ScanDatasetMetadata>>: parse可能なmetadata。致命的なparse失敗時はstd::nullopt。
    std::optional<ScanDatasetMetadata> readMetadata(
        const std::filesystem::path& metadata_path,
        std::vector<ScanDatasetIssue>& issues) const;

    /// @brief metadata上のpattern_countに基づいてexpected imageを検証する。
    ///
    /// Args:
    ///   input_dir <const std::filesystem::path&>: scan dataset directory。
    ///   metadata <const ScanDatasetMetadata&>: metadataから得たexpected値。
    ///   result <ScanDatasetValidationResult&>: validation集計結果。
    ///
    /// Return:
    ///   <void>: 戻り値なし。
    void validateExpectedImages(
        const std::filesystem::path& left_dir,
        const std::filesystem::path& right_dir,
        int pattern_count,
        ScanDatasetValidationResult& result) const;
};

} // namespace scan::dataset
