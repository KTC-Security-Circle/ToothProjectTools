#include "service/scan_dataset_validator.hpp"

#include <opencv2/core.hpp>
#include <opencv2/core/persistence.hpp>
#include <opencv2/imgcodecs.hpp>

#include <iomanip>
#include <sstream>
#include <string>

namespace service::scan_dataset
{
namespace
{

ScanDatasetIssue issue(std::string code, std::string message, const std::filesystem::path& path = {}, int pattern_index = -1)
{
    return ScanDatasetIssue{std::move(code), std::move(message), path.empty() ? std::string{} : path.string(), pattern_index};
}

std::string patternFileName(int index)
{
    std::ostringstream stream;
    stream << "pattern_" << std::setfill('0') << std::setw(3) << index << ".png";
    return stream.str();
}

std::optional<std::string> readString(const cv::FileNode& node, const char* key)
{
    const auto value = node[key];
    if (value.empty() || !value.isString())
    {
        return std::nullopt;
    }
    return static_cast<std::string>(value);
}

int readIntOrZero(const cv::FileNode& node, const char* key)
{
    const auto value = node[key];
    if (value.empty() || !value.isInt())
    {
        return 0;
    }
    return static_cast<int>(value);
}

bool onlyPartialMissingIssues(const std::vector<ScanDatasetIssue>& issues)
{
    if (issues.empty())
    {
        return false;
    }
    for (const auto& current : issues)
    {
        if (current.code != "missing_left_image" && current.code != "missing_right_image")
        {
            return false;
        }
    }
    return true;
}

void setValidity(ScanDatasetValidationResult& result, bool allow_partial)
{
    result.partial = result.missing_count > 0 && (result.left_count > 0 || result.right_count > 0);
    result.valid = result.issues.empty() || (allow_partial && onlyPartialMissingIssues(result.issues));
}

} // namespace

ScanDatasetValidationResult ScanDatasetValidator::validate(const ScanDatasetValidationConfig& config) const
{
    ScanDatasetValidationResult result;
    result.input_dir = config.input_dir.string();

    std::error_code error_code;
    if (!std::filesystem::exists(config.input_dir, error_code))
    {
        result.issues.push_back(issue("input_dir_not_found", "input_dir does not exist", config.input_dir));
        setValidity(result, config.allow_partial);
        return result;
    }
    if (!std::filesystem::is_directory(config.input_dir, error_code))
    {
        result.issues.push_back(issue("input_dir_not_directory", "input_dir is not a directory", config.input_dir));
        setValidity(result, config.allow_partial);
        return result;
    }

    const auto metadata_path = config.input_dir / "metadata.json";
    const auto metadata = readMetadata(metadata_path, result.issues);
    if (!metadata)
    {
        setValidity(result, config.allow_partial);
        return result;
    }

    result.scan_id = metadata->scan_id;
    result.pattern_count = metadata->pattern_count;

    const auto left_dir = config.input_dir / "left";
    const auto right_dir = config.input_dir / "right";
    if (!std::filesystem::exists(left_dir, error_code) || !std::filesystem::is_directory(left_dir, error_code))
    {
        result.issues.push_back(issue("left_dir_not_found", "left directory does not exist", left_dir));
    }
    if (!std::filesystem::exists(right_dir, error_code) || !std::filesystem::is_directory(right_dir, error_code))
    {
        result.issues.push_back(issue("right_dir_not_found", "right directory does not exist", right_dir));
    }

    if (metadata->pattern_count > 0 && std::filesystem::is_directory(left_dir, error_code) &&
        std::filesystem::is_directory(right_dir, error_code))
    {
        validateExpectedImages(config.input_dir, *metadata, result);
    }

    setValidity(result, config.allow_partial);
    return result;
}

std::optional<ScanDatasetMetadata> ScanDatasetValidator::readMetadataForDecode(
    const std::filesystem::path& input_dir,
    std::vector<ScanDatasetIssue>& issues) const
{
    return readMetadata(input_dir / "metadata.json", issues);
}

std::optional<ScanDatasetMetadata> ScanDatasetValidator::readMetadata(
    const std::filesystem::path& metadata_path,
    std::vector<ScanDatasetIssue>& issues) const
{
    std::error_code error_code;
    if (!std::filesystem::exists(metadata_path, error_code))
    {
        issues.push_back(issue("metadata_not_found", "metadata.json does not exist", metadata_path));
        return std::nullopt;
    }

    try
    {
        cv::FileStorage storage(metadata_path.string(), cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
        if (!storage.isOpened())
        {
            issues.push_back(issue("metadata_read_failed", "failed to read metadata.json", metadata_path));
            return std::nullopt;
        }

        const auto root = storage.root();
        if (root.empty() || !root.isMap())
        {
            issues.push_back(issue("metadata_parse_failed", "metadata.json must be an object", metadata_path));
            return std::nullopt;
        }

        ScanDatasetMetadata metadata;
        metadata.scan_id = readString(root, "scan_id").value_or("");
        metadata.projector_role = readString(root, "projector_role").value_or("");
        metadata.left_role = readString(root, "left_role").value_or("");
        metadata.right_role = readString(root, "right_role").value_or("");
        metadata.output_dir = readString(root, "output_dir").value_or("");
        metadata.pattern_count = readIntOrZero(root, "pattern_count");
        metadata.settle_ms = readIntOrZero(root, "settle_ms");

        const auto surface = root["surface"];
        if (!surface.empty() && surface.isMap())
        {
            metadata.surface_width = readIntOrZero(surface, "surface_width");
            metadata.surface_height = readIntOrZero(surface, "surface_height");
            metadata.pattern_width = readIntOrZero(surface, "pattern_width");
            metadata.pattern_height = readIntOrZero(surface, "pattern_height");
            metadata.pattern_x = readIntOrZero(surface, "pattern_x");
            metadata.pattern_y = readIntOrZero(surface, "pattern_y");
        }

        if (metadata.scan_id.empty())
        {
            issues.push_back(issue("metadata_missing_scan_id", "metadata scan_id is missing", metadata_path));
        }
        if (metadata.pattern_count <= 0)
        {
            issues.push_back(issue("metadata_invalid_pattern_count", "metadata pattern_count must be positive", metadata_path));
        }
        if (metadata.pattern_width <= 0 || metadata.pattern_height <= 0)
        {
            issues.push_back(issue("metadata_invalid_surface", "metadata surface pattern size must be positive", metadata_path));
        }

        return metadata;
    }
    catch (const cv::Exception&)
    {
        issues.push_back(issue("metadata_parse_failed", "failed to parse metadata.json", metadata_path));
        return std::nullopt;
    }
    catch (const std::exception&)
    {
        issues.push_back(issue("metadata_read_failed", "failed to read metadata.json", metadata_path));
        return std::nullopt;
    }
}

void ScanDatasetValidator::validateExpectedImages(
    const std::filesystem::path& input_dir,
    const ScanDatasetMetadata& metadata,
    ScanDatasetValidationResult& result) const
{
    cv::Size expected_size;

    for (int index = 0; index < metadata.pattern_count; ++index)
    {
        const auto left_path = input_dir / "left" / patternFileName(index);
        const auto right_path = input_dir / "right" / patternFileName(index);
        std::error_code error_code;
        const bool left_exists = std::filesystem::exists(left_path, error_code);
        const bool right_exists = std::filesystem::exists(right_path, error_code);

        if (!left_exists || !right_exists)
        {
            ++result.missing_count;
        }
        if (!left_exists)
        {
            result.issues.push_back(issue("missing_left_image", "missing left image for pattern index " + std::to_string(index), left_path, index));
        }
        if (!right_exists)
        {
            result.issues.push_back(issue("missing_right_image", "missing right image for pattern index " + std::to_string(index), right_path, index));
        }

        cv::Mat left;
        cv::Mat right;
        bool left_ok = false;
        bool right_ok = false;

        if (left_exists)
        {
            if (!cv::haveImageReader(left_path.string()))
            {
                result.issues.push_back(issue("unreadable_left_image", "left image is unreadable for pattern index " + std::to_string(index), left_path, index));
            }
            else
            {
                left = cv::imread(left_path.string(), cv::IMREAD_UNCHANGED);
                if (left.empty())
                {
                    result.issues.push_back(issue("empty_left_image", "left image is empty for pattern index " + std::to_string(index), left_path, index));
                }
                else
                {
                    left_ok = true;
                    ++result.left_count;
                }
            }
        }
        if (right_exists)
        {
            if (!cv::haveImageReader(right_path.string()))
            {
                result.issues.push_back(issue("unreadable_right_image", "right image is unreadable for pattern index " + std::to_string(index), right_path, index));
            }
            else
            {
                right = cv::imread(right_path.string(), cv::IMREAD_UNCHANGED);
                if (right.empty())
                {
                    result.issues.push_back(issue("empty_right_image", "right image is empty for pattern index " + std::to_string(index), right_path, index));
                }
                else
                {
                    right_ok = true;
                    ++result.right_count;
                }
            }
        }

        if (left_ok && right_ok && left.size() != right.size())
        {
            result.issues.push_back(issue("stereo_size_mismatch", "left/right image sizes differ for pattern index " + std::to_string(index), left_path, index));
        }

        const cv::Size size_for_index = left_ok ? left.size() : (right_ok ? right.size() : cv::Size{});
        if (size_for_index.width > 0 && size_for_index.height > 0)
        {
            if (expected_size.width == 0 || expected_size.height == 0)
            {
                expected_size = size_for_index;
                result.width = expected_size.width;
                result.height = expected_size.height;
            }
            else if (size_for_index != expected_size)
            {
                result.issues.push_back(issue("image_size_inconsistent", "image size differs from first readable image for pattern index " + std::to_string(index), left_ok ? left_path : right_path, index));
            }
        }
    }
}

} // namespace service::scan_dataset
