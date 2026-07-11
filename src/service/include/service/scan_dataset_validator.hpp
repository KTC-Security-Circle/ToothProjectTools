#pragma once

#include "service/scan_dataset_result.hpp"

#include <filesystem>
#include <optional>
#include <vector>

namespace service::scan_dataset
{

struct ScanDatasetValidationConfig
{
    /// input_dir <std::filesystem::path>: scan dataset directory。
    std::filesystem::path input_dir;

    /// allow_partial <bool>: 欠損画像があるpartial datasetをvalid扱いするか。
    bool allow_partial{false};
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
        const std::filesystem::path& input_dir,
        const ScanDatasetMetadata& metadata,
        ScanDatasetValidationResult& result) const;
};

} // namespace service::scan_dataset
