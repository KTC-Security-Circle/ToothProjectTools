#include "service/decode_service.hpp"

#include "service/scan_dataset_validator.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <sstream>
#include <utility>

namespace service::decode
{
namespace
{

int ceilLog2(int value)
{
    int bits = 0;
    int current = 1;
    while (current < value)
    {
        current <<= 1;
        ++bits;
    }
    return bits;
}

int grayToBinary(int gray)
{
    int binary = gray;
    while (gray >>= 1)
    {
        binary ^= gray;
    }
    return binary;
}

std::string patternFileName(int index)
{
    std::ostringstream stream;
    stream << "pattern_" << std::setfill('0') << std::setw(3) << index << ".png";
    return stream.str();
}

std::string jsonEscape(const std::string& value)
{
    std::string escaped;
    for (const auto ch : value)
    {
        switch (ch)
        {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped.push_back(ch);
            break;
        }
    }
    return escaped;
}

std::string issuesSummary(const std::vector<service::scan_dataset::ScanDatasetIssue>& issues)
{
    std::ostringstream stream;
    stream << "scan dataset is not valid";
    if (!issues.empty())
    {
        stream << ": ";
        for (std::size_t index = 0; index < issues.size(); ++index)
        {
            if (index > 0)
            {
                stream << ", ";
            }
            stream << issues[index].code;
        }
    }
    return stream.str();
}

bool writeMatYml(const std::filesystem::path& path, const std::string& key, const cv::Mat& mat)
{
    cv::FileStorage storage(path.string(), cv::FileStorage::WRITE);
    if (!storage.isOpened())
    {
        return false;
    }
    storage << key << mat;
    return true;
}

} // namespace

DecodeService::DecodeService(service::scan_dataset::ScanDatasetValidator& validator) : validator_(validator) {}

DecodePatternsResult DecodeService::decodePatterns(const DecodePatternsConfig& config) const
{
    auto result = DecodePatternsResult::success();
    result.input_dir = config.input_dir ? config.input_dir->string() : std::string{};
    result.output_dir = config.output_dir.string();
    result.threshold = config.threshold;

    service::scan_dataset::ScanDatasetResolver resolver;
    const auto resolved = resolver.resolve(config);
    if (!resolved.ok)
    {
        auto failure = DecodePatternsResult::failure("scan_dataset_invalid", issuesSummary(resolved.issues));
        failure.input_dir = result.input_dir;
        failure.output_dir = result.output_dir;
        failure.threshold = config.threshold;
        return failure;
    }
    result.input_dir = resolved.dataset.root_dir.string();

    service::scan_dataset::ScanDatasetValidationConfig validation_config;
    validation_config.input_dir = config.input_dir;
    validation_config.left_dir = config.left_dir;
    validation_config.right_dir = config.right_dir;
    validation_config.metadata_file = config.metadata_file;
    validation_config.allow_partial = config.allow_partial;
    validation_config.projector_width = config.projector_width;
    validation_config.projector_height = config.projector_height;
    validation_config.pattern_count = config.pattern_count;
    const auto validation = validator_.validate(validation_config);
    const bool single_camera = resolved.dataset.right_dir.empty() || !std::filesystem::is_directory(resolved.dataset.right_dir);
    if (!validation.valid || (!single_camera && config.allow_partial && std::min(validation.left_count, validation.right_count) == 0))
    {
        auto failure = DecodePatternsResult::failure("scan_dataset_invalid", issuesSummary(validation.issues));
        failure.input_dir = result.input_dir;
        failure.output_dir = result.output_dir;
        failure.threshold = config.threshold;
        return failure;
    }

    std::vector<service::scan_dataset::ScanDatasetIssue> metadata_issues;
    std::optional<service::scan_dataset::ScanDatasetMetadata> metadata;
    if (resolved.dataset.metadata_file)
    {
        metadata = validator_.readMetadataFileForDecode(*resolved.dataset.metadata_file, metadata_issues);
        if (!metadata)
        {
            auto failure = DecodePatternsResult::failure("scan_dataset_invalid", issuesSummary(metadata_issues));
            failure.input_dir = result.input_dir;
            failure.output_dir = result.output_dir;
            failure.threshold = config.threshold;
            return failure;
        }
    }

    service::scan_dataset::ScanDatasetMetadata metadata_for_output;
    if (metadata)
    {
        metadata_for_output = *metadata;
        result.scan_id = metadata->scan_id;
        result.pattern_count = metadata->pattern_count;
        result.projector_width = metadata->projector_width;
        result.projector_height = metadata->projector_height;
    }
    else
    {
        if (!config.projector_width || !config.projector_height || *config.projector_width <= 0 ||
            *config.projector_height <= 0)
        {
            auto failure = DecodePatternsResult::failure(
                "scan_dataset_invalid", "metadata.json is missing and projector_width/projector_height are required");
            failure.input_dir = result.input_dir;
            failure.output_dir = result.output_dir;
            failure.threshold = config.threshold;
            return failure;
        }
        result.scan_id = resolved.dataset.root_dir.filename().string();
        result.pattern_count = config.pattern_count.value_or(resolved.dataset.pattern_count);
        result.projector_width = *config.projector_width;
        result.projector_height = *config.projector_height;
        metadata_for_output.scan_id = result.scan_id;
        metadata_for_output.pattern_count = result.pattern_count;
        metadata_for_output.pattern_width = result.projector_width;
        metadata_for_output.pattern_height = result.projector_height;
        metadata_for_output.projector_width = result.projector_width;
        metadata_for_output.projector_height = result.projector_height;
        metadata_for_output.display_width = result.projector_width;
        metadata_for_output.display_height = result.projector_height;
        metadata_for_output.surface_width = result.projector_width;
        metadata_for_output.surface_height = result.projector_height;
    }

    const int x_bits = ceilLog2(result.projector_width);
    const int y_bits = ceilLog2(result.projector_height);
    const int stripe_pattern_count = 2 * (x_bits + y_bits);
    const bool has_shadow_patterns = result.pattern_count == stripe_pattern_count + 2;
    if (result.pattern_count != stripe_pattern_count && !has_shadow_patterns)
    {
        auto failure = DecodePatternsResult::failure("decode_pattern_count_mismatch",
                                                     "pattern_count does not match GrayCode decoder expected count");
        failure.input_dir = result.input_dir;
        failure.output_dir = result.output_dir;
        failure.scan_id = result.scan_id;
        failure.pattern_count = result.pattern_count;
        failure.projector_width = result.projector_width;
        failure.projector_height = result.projector_height;
        failure.threshold = config.threshold;
        return failure;
    }

    std::string error_message;
    const auto left_patterns = loadPatternImages(resolved.dataset.left_dir, stripe_pattern_count, error_message);
    if (!error_message.empty())
    {
        auto failure = DecodePatternsResult::failure("decode_image_load_failed", error_message);
        failure.input_dir = result.input_dir;
        failure.output_dir = result.output_dir;
        failure.scan_id = result.scan_id;
        failure.pattern_count = result.pattern_count;
        failure.threshold = config.threshold;
        return failure;
    }
    std::vector<cv::Mat> right_patterns;
    if (!single_camera)
        right_patterns = loadPatternImages(resolved.dataset.right_dir, stripe_pattern_count, error_message);
    if (!error_message.empty())
    {
        auto failure = DecodePatternsResult::failure("decode_image_load_failed", error_message);
        failure.input_dir = result.input_dir;
        failure.output_dir = result.output_dir;
        failure.scan_id = result.scan_id;
        failure.pattern_count = result.pattern_count;
        failure.threshold = config.threshold;
        return failure;
    }

    DecodeSideResult left;
    DecodeSideResult right;
    try
    {
        left = decodeSide(left_patterns, result.projector_width, result.projector_height, config.threshold);
        if (!single_camera)
            right = decodeSide(right_patterns, result.projector_width, result.projector_height, config.threshold);
    }
    catch (const std::invalid_argument& error)
    {
        auto failure = DecodePatternsResult::failure("decode_image_size_mismatch", error.what());
        failure.input_dir = result.input_dir;
        failure.output_dir = result.output_dir;
        failure.scan_id = result.scan_id;
        failure.pattern_count = result.pattern_count;
        failure.threshold = config.threshold;
        return failure;
    }
    catch (const std::exception& error)
    {
        auto failure = DecodePatternsResult::failure("decode_failed", error.what());
        failure.input_dir = result.input_dir;
        failure.output_dir = result.output_dir;
        failure.scan_id = result.scan_id;
        failure.pattern_count = result.pattern_count;
        failure.threshold = config.threshold;
        return failure;
    }

    result.image_width = left.image_width;
    result.image_height = left.image_height;
    result.left_valid_count = left.valid_count;
    result.right_valid_count = single_camera ? 0 : right.valid_count;
    result.left_valid_ratio = left.valid_ratio;
    result.right_valid_ratio = single_camera ? 0.0 : right.valid_ratio;

    if (!single_camera && (left.image_width != right.image_width || left.image_height != right.image_height))
    {
        auto failure =
            DecodePatternsResult::failure("decode_image_size_mismatch", "left/right decoded image sizes differ");
        failure.input_dir = result.input_dir;
        failure.output_dir = result.output_dir;
        failure.scan_id = result.scan_id;
        failure.pattern_count = result.pattern_count;
        failure.threshold = config.threshold;
        return failure;
    }

    if (!writeDecodeOutput(config.output_dir, "left", left, error_message) ||
        (!single_camera && !writeDecodeOutput(config.output_dir, "right", right, error_message)) ||
        !writeMetadata(config, result, metadata_for_output, error_message))
    {
        auto failure = DecodePatternsResult::failure("decode_output_write_failed", error_message);
        failure.input_dir = result.input_dir;
        failure.output_dir = result.output_dir;
        failure.scan_id = result.scan_id;
        failure.pattern_count = result.pattern_count;
        failure.image_width = result.image_width;
        failure.image_height = result.image_height;
        failure.projector_width = result.projector_width;
        failure.projector_height = result.projector_height;
        failure.threshold = config.threshold;
        return failure;
    }

    return result;
}

DecodeSideResult DecodeService::decodeSide(const std::vector<cv::Mat>& patterns, int projector_width,
                                           int projector_height, int threshold) const
{
    if (patterns.empty())
    {
        throw std::invalid_argument("pattern list is empty");
    }

    std::vector<cv::Mat> gray_patterns;
    gray_patterns.reserve(patterns.size());
    const cv::Size image_size = patterns.front().size();
    for (const auto& pattern : patterns)
    {
        if (pattern.size() != image_size)
        {
            throw std::invalid_argument("captured pattern image sizes differ");
        }
        cv::Mat gray;
        if (pattern.channels() == 1)
        {
            gray = pattern;
        }
        else
        {
            cv::cvtColor(pattern, gray, cv::COLOR_BGR2GRAY);
        }
        gray_patterns.push_back(gray);
    }

    const int x_bits = ceilLog2(projector_width);
    const int y_bits = ceilLog2(projector_height);
    const int expected_count = 2 * (x_bits + y_bits);
    if (static_cast<int>(gray_patterns.size()) < expected_count)
    {
        throw std::invalid_argument("not enough captured pattern images");
    }

    DecodeSideResult result;
    result.image_width = image_size.width;
    result.image_height = image_size.height;
    result.projector_x = cv::Mat(image_size, CV_32SC1, cv::Scalar(-1));
    result.projector_y = cv::Mat(image_size, CV_32SC1, cv::Scalar(-1));
    result.valid_mask = cv::Mat(image_size, CV_8UC1, cv::Scalar(0));

    for (int y = 0; y < image_size.height; ++y)
    {
        for (int x = 0; x < image_size.width; ++x)
        {
            bool valid = true;
            int gray_x = 0;
            int gray_y = 0;
            int pattern_index = 0;

            for (int bit = 0; bit < x_bits; ++bit)
            {
                const int normal = gray_patterns[pattern_index].at<uchar>(y, x);
                const int inverse = gray_patterns[pattern_index + 1].at<uchar>(y, x);
                pattern_index += 2;
                if (std::abs(normal - inverse) < threshold)
                {
                    valid = false;
                    break;
                }
                gray_x = (gray_x << 1) | (normal > inverse ? 1 : 0);
            }

            for (int bit = 0; valid && bit < y_bits; ++bit)
            {
                const int normal = gray_patterns[pattern_index].at<uchar>(y, x);
                const int inverse = gray_patterns[pattern_index + 1].at<uchar>(y, x);
                pattern_index += 2;
                if (std::abs(normal - inverse) < threshold)
                {
                    valid = false;
                    break;
                }
                gray_y = (gray_y << 1) | (normal > inverse ? 1 : 0);
            }

            if (!valid)
            {
                continue;
            }

            const int projector_x = grayToBinary(gray_x);
            const int projector_y = grayToBinary(gray_y);
            if (projector_x < 0 || projector_x >= projector_width || projector_y < 0 || projector_y >= projector_height)
            {
                continue;
            }

            result.projector_x.at<int>(y, x) = projector_x;
            result.projector_y.at<int>(y, x) = projector_y;
            result.valid_mask.at<uchar>(y, x) = 255;
            ++result.valid_count;
        }
    }

    const auto total = static_cast<double>(image_size.area());
    result.valid_ratio = total > 0.0 ? static_cast<double>(result.valid_count) / total : 0.0;
    return result;
}

std::vector<cv::Mat> DecodeService::loadPatternImages(const std::filesystem::path& side_dir, int pattern_count,
                                                      std::string& error_message) const
{
    error_message.clear();
    std::vector<cv::Mat> images;
    images.reserve(static_cast<std::size_t>(pattern_count));
    for (int index = 0; index < pattern_count; ++index)
    {
        const auto path = side_dir / patternFileName(index);
        auto image = cv::imread(path.string(), cv::IMREAD_UNCHANGED);
        if (image.empty())
        {
            error_message = "failed to load pattern image: " + path.string();
            return {};
        }
        images.push_back(std::move(image));
    }
    return images;
}

bool DecodeService::writeDecodeOutput(const std::filesystem::path& output_dir, const std::string& side,
                                      const DecodeSideResult& result, std::string& error_message) const
{
    try
    {
        const auto side_dir = output_dir / side;
        std::filesystem::create_directories(side_dir);
        if (!writeMatYml(side_dir / "projector_x.yml", "projector_x", result.projector_x))
        {
            error_message = "failed to write projector_x.yml";
            return false;
        }
        if (!writeMatYml(side_dir / "projector_y.yml", "projector_y", result.projector_y))
        {
            error_message = "failed to write projector_y.yml";
            return false;
        }
        if (!cv::imwrite((side_dir / "valid_mask.png").string(), result.valid_mask))
        {
            error_message = "failed to write valid_mask.png";
            return false;
        }
        return true;
    }
    catch (const std::exception& error)
    {
        error_message = error.what();
        return false;
    }
}

bool DecodeService::writeMetadata(const DecodePatternsConfig& config, const DecodePatternsResult& result,
                                  const service::scan_dataset::ScanDatasetMetadata& metadata,
                                  std::string& error_message) const
{
    try
    {
        std::filesystem::create_directories(config.output_dir);
        std::ofstream output(config.output_dir / "metadata.json");
        if (!output)
        {
            error_message = "failed to open decode metadata.json";
            return false;
        }

        output << std::fixed << std::setprecision(6);
        output << "{\n"
               << "  \"version\": \"0.1.0\",\n"
               << "  \"scan_id\": \"" << jsonEscape(result.scan_id) << "\",\n"
               << "  \"input_dir\": \"" << jsonEscape(result.input_dir) << "\",\n"
               << "  \"output_dir\": \"" << jsonEscape(config.output_dir.string()) << "\",\n"
               << "  \"pattern_count\": " << result.pattern_count << ",\n"
               << "  \"image_width\": " << result.image_width << ",\n"
               << "  \"image_height\": " << result.image_height << ",\n"
               << "  \"projector_width\": " << result.projector_width << ",\n"
               << "  \"projector_height\": " << result.projector_height << ",\n"
               << "  \"code_width\": " << result.projector_width << ",\n"
               << "  \"code_height\": " << result.projector_height << ",\n"
               << "  \"threshold\": " << result.threshold << ",\n"
               << "  \"left_valid_count\": " << result.left_valid_count << ",\n"
               << "  \"right_valid_count\": " << result.right_valid_count << ",\n"
               << "  \"left_valid_ratio\": " << result.left_valid_ratio << ",\n"
               << "  \"right_valid_ratio\": " << result.right_valid_ratio << ",\n"
               << "  \"surface\": {\n"
               << "    \"surface_width\": " << metadata.surface_width << ",\n"
               << "    \"surface_height\": " << metadata.surface_height << ",\n"
               << "    \"display_width\": " << metadata.display_width << ",\n"
               << "    \"display_height\": " << metadata.display_height << ",\n"
               << "    \"display_x\": " << metadata.display_x << ",\n"
               << "    \"display_y\": " << metadata.display_y << ",\n"
               << "    \"pattern_width\": " << metadata.pattern_width << ",\n"
               << "    \"pattern_height\": " << metadata.pattern_height << ",\n"
               << "    \"pattern_x\": " << metadata.pattern_x << ",\n"
               << "    \"pattern_y\": " << metadata.pattern_y << "\n"
               << "  }\n"
               << "}\n";
        return true;
    }
    catch (const std::exception& error)
    {
        error_message = error.what();
        return false;
    }
}

} // namespace service::decode
