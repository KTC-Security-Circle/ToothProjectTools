#pragma once

#include <opencv2/core/mat.hpp>

#include <optional>
#include <string>

namespace decode
{

struct DecodeSideResult
{
    /// projector_x <cv::Mat>: camera pixelごとのprojector x座標。CV_32SC1。
    cv::Mat projector_x;

    /// projector_y <cv::Mat>: camera pixelごとのprojector y座標。CV_32SC1。
    cv::Mat projector_y;

    /// valid_mask <cv::Mat>: valid pixel mask。CV_8UC1。
    cv::Mat valid_mask;

    /// valid_count <int>: valid pixel数。
    int valid_count{0};

    /// valid_ratio <double>: valid pixel率。
    double valid_ratio{0.0};

    /// image_width <int>: input captured image width。
    int image_width{0};

    /// image_height <int>: input captured image height。
    int image_height{0};
};

struct DecodeError
{
    /// code <std::string>: decode command失敗時のerror code。
    std::string code;

    /// message <std::string>: decode command失敗時のerror message。
    std::string message;
};

struct DecodePatternsResult
{
    /// ok <bool>: decodeが成功したか。
    bool ok{false};

    /// input_dir <std::string>: scan dataset input directory。
    std::string input_dir;

    /// output_dir <std::string>: decode output directory。
    std::string output_dir;

    /// scan_id <std::string>: scan session識別子。
    std::string scan_id;

    /// pattern_count <int>: input pattern count。
    int pattern_count{0};

    /// image_width <int>: captured image width。
    int image_width{0};

    /// image_height <int>: captured image height。
    int image_height{0};

    /// projector_width <int>: active projector pattern width。
    int projector_width{0};

    /// projector_height <int>: active projector pattern height。
    int projector_height{0};

    /// threshold <int>: decode threshold。
    int threshold{0};

    /// left_valid_count <int>: left decode valid pixel数。
    int left_valid_count{0};

    /// right_valid_count <int>: right decode valid pixel数。
    int right_valid_count{0};

    /// left_valid_ratio <double>: left decode valid pixel率。
    double left_valid_ratio{0.0};

    /// right_valid_ratio <double>: right decode valid pixel率。
    double right_valid_ratio{0.0};

    /// error <std::optional<DecodeError>>: 失敗時のerror情報。
    std::optional<DecodeError> error;

    /// @brief decode成功結果を作成する。
    static DecodePatternsResult success();

    /// @brief decode失敗結果を作成する。
    static DecodePatternsResult failure(std::string code, std::string message);
};

} // namespace decode
