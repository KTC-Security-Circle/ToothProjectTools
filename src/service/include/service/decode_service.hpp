#pragma once

#include "service/decode_result.hpp"
#include "service/scan_dataset_result.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace service::scan_dataset
{
class ScanDatasetValidator;
}

namespace service::decode
{

struct DecodePatternsConfig
{
    /// input_dir <std::filesystem::path>: scan dataset directory。
    std::filesystem::path input_dir;

    /// output_dir <std::filesystem::path>: decode result output directory。
    std::filesystem::path output_dir;

    /// threshold <int>: GrayCode inverse pairの明暗差threshold。
    int threshold{15};

    /// allow_partial <bool>: partial datasetをdecode対象として許可するか。
    bool allow_partial{false};
};

class DecodeService
{
  public:
    /// @brief DecodeServiceを構築する。
    ///
    /// Args:
    ///   validator <service::scan_dataset::ScanDatasetValidator&>: scan dataset validator。
    explicit DecodeService(service::scan_dataset::ScanDatasetValidator& validator);

    /// @brief scan datasetからGrayCode patternをdecodeする。
    ///
    /// Args:
    ///   config <const DecodePatternsConfig&>: decode設定。
    ///
    /// Return:
    ///   <DecodePatternsResult>: decode結果。
    DecodePatternsResult decodePatterns(const DecodePatternsConfig& config) const;

  private:
    /// @brief 片側cameraのcaptured GrayCode patternをprojector座標mapへdecodeする。
    DecodeSideResult decodeSide(
        const std::vector<cv::Mat>& patterns,
        int projector_width,
        int projector_height,
        int threshold) const;

    /// @brief pattern_NNN.pngを順番に読み込む。
    std::vector<cv::Mat> loadPatternImages(
        const std::filesystem::path& side_dir,
        int pattern_count,
        std::string& error_message) const;

    /// @brief 片側decode結果をprojector_x/projector_y/valid_maskとして保存する。
    bool writeDecodeOutput(
        const std::filesystem::path& output_dir,
        const std::string& side,
        const DecodeSideResult& result,
        std::string& error_message) const;

    /// @brief decode metadata.jsonを保存する。
    bool writeMetadata(
        const DecodePatternsConfig& config,
        const DecodePatternsResult& result,
        const service::scan_dataset::ScanDatasetMetadata& metadata,
        std::string& error_message) const;

    /// validator_ <service::scan_dataset::ScanDatasetValidator&>: 入力scan dataset検証service。
    service::scan_dataset::ScanDatasetValidator& validator_;
};

} // namespace service::decode
