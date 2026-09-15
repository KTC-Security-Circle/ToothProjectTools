#include "scan/scan_dataset_validator.hpp"

#include <iomanip>
#include <opencv2/core.hpp>
#include <opencv2/core/persistence.hpp>
#include <opencv2/imgcodecs.hpp>
#include <sstream>
#include <string>

namespace service::scan_dataset
{
namespace
{

ScanDatasetIssue issue(std::string code, std::string message, const std::filesystem::path& path = {},
                       int pattern_index = -1)
{
    return ScanDatasetIssue{std::move(code), std::move(message), path.empty() ? std::string{} : path.string(),
                            pattern_index};
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

    ScanDatasetResolver resolver;
    const auto resolved = resolver.resolve(config);
    result.issues.insert(result.issues.end(), resolved.issues.begin(), resolved.issues.end());
    result.input_dir = resolved.dataset.root_dir.string();

    std::optional<ScanDatasetMetadata> metadata;
    if (resolved.dataset.metadata_file)
    {
        metadata = readMetadata(*resolved.dataset.metadata_file, result.issues);
    }

    if (metadata)
    {
        result.scan_id = metadata->scan_id;
        result.pattern_count = metadata->pattern_count;
    }
    else
    {
        result.pattern_count = config.pattern_count.value_or(resolved.dataset.pattern_count);
    }

    if (result.pattern_count > 0 && resolved.ok)
    {
        validateExpectedImages(resolved.dataset.left_dir, resolved.dataset.right_dir, result.pattern_count, result);
    }
    else if (result.pattern_count <= 0 && resolved.ok)
    {
        result.issues.push_back(
            issue("pattern_count_not_found", "pattern_count could not be inferred", resolved.dataset.root_dir));
    }

    setValidity(result, config.allow_partial);
    return result;
}

std::optional<ScanDatasetMetadata>
ScanDatasetValidator::readMetadataForDecode(const std::filesystem::path& input_dir,
                                            std::vector<ScanDatasetIssue>& issues) const
{
    return readMetadata(input_dir / "metadata.json", issues);
}

std::optional<ScanDatasetMetadata>
ScanDatasetValidator::readMetadataFileForDecode(const std::filesystem::path& metadata_file,
                                                std::vector<ScanDatasetIssue>& issues) const
{
    return readMetadata(metadata_file, issues);
}

std::optional<ScanDatasetMetadata> ScanDatasetValidator::readMetadata(const std::filesystem::path& metadata_path,
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
        metadata.projector_width = readIntOrZero(root, "projector_width");
        metadata.projector_height = readIntOrZero(root, "projector_height");
        metadata.settle_ms = readIntOrZero(root, "settle_ms");
        metadata.sync_source = readString(root, "sync_source").value_or("");
        metadata.roi_x = readIntOrZero(root, "roi_x");
        metadata.roi_y = readIntOrZero(root, "roi_y");
        metadata.roi_width = readIntOrZero(root, "roi_width");
        metadata.roi_height = readIntOrZero(root, "roi_height");
        metadata.roi_decode_margin = readIntOrZero(root, "roi_decode_margin");

        const auto surface = root["surface"];
        if (!surface.empty() && surface.isMap())
        {
            metadata.surface_width = readIntOrZero(surface, "surface_width");
            metadata.surface_height = readIntOrZero(surface, "surface_height");
            metadata.pattern_width = readIntOrZero(surface, "pattern_width");
            metadata.pattern_height = readIntOrZero(surface, "pattern_height");
            metadata.pattern_x = readIntOrZero(surface, "pattern_x");
            metadata.pattern_y = readIntOrZero(surface, "pattern_y");
            metadata.display_width = readIntOrZero(surface, "display_width");
            metadata.display_height = readIntOrZero(surface, "display_height");
            metadata.display_x = readIntOrZero(surface, "display_x");
            metadata.display_y = readIntOrZero(surface, "display_y");
            if (metadata.display_width <= 0)
            {
                metadata.display_width = metadata.pattern_width;
            }
            if (metadata.display_height <= 0)
            {
                metadata.display_height = metadata.pattern_height;
            }
            if (metadata.display_x == 0)
            {
                metadata.display_x = metadata.pattern_x;
            }
            if (metadata.display_y == 0)
            {
                metadata.display_y = metadata.pattern_y;
            }
        }

        if (metadata.projector_width <= 0)
        {
            metadata.projector_width = metadata.pattern_width;
        }
        if (metadata.projector_height <= 0)
        {
            metadata.projector_height = metadata.pattern_height;
        }

        if (metadata.scan_id.empty())
        {
            issues.push_back(issue("metadata_missing_scan_id", "metadata scan_id is missing", metadata_path));
        }
        if (metadata.pattern_count <= 0)
        {
            issues.push_back(
                issue("metadata_invalid_pattern_count", "metadata pattern_count must be positive", metadata_path));
        }
        if (metadata.projector_width <= 0 || metadata.projector_height <= 0)
        {
            issues.push_back(
                issue("metadata_invalid_surface", "metadata surface pattern size must be positive", metadata_path));
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

void ScanDatasetValidator::validateExpectedImages(const std::filesystem::path& left_dir,
                                                  const std::filesystem::path& right_dir, int pattern_count,
                                                  ScanDatasetValidationResult& result) const
{
    cv::Size expected_size;

    for (int index = 0; index < pattern_count; ++index)
    {
        const auto left_path = left_dir / patternFileName(index);
        const auto right_path = right_dir / patternFileName(index);
        std::error_code error_code;
        const bool left_exists = std::filesystem::exists(left_path, error_code);
        const bool right_exists = std::filesystem::exists(right_path, error_code);

        const bool single_camera = right_dir.empty();
        if ((!left_exists && !single_camera) || (!right_exists && !single_camera))
        {
            ++result.missing_count;
        }
        if (!left_exists)
        {
            result.issues.push_back(issue("missing_left_image",
                                          "missing left image for pattern index " + std::to_string(index), left_path,
                                          index));
        }
        if (!right_exists && !single_camera)
        {
            result.issues.push_back(issue("missing_right_image",
                                          "missing right image for pattern index " + std::to_string(index), right_path,
                                          index));
        }

        cv::Mat left;
        cv::Mat right;
        bool left_ok = false;
        bool right_ok = false;

        if (left_exists)
        {
            if (!cv::haveImageReader(left_path.string()))
            {
                result.issues.push_back(issue("unreadable_left_image",
                                              "left image is unreadable for pattern index " + std::to_string(index),
                                              left_path, index));
            }
            else
            {
                left = cv::imread(left_path.string(), cv::IMREAD_UNCHANGED);
                if (left.empty())
                {
                    result.issues.push_back(issue("empty_left_image",
                                                  "left image is empty for pattern index " + std::to_string(index),
                                                  left_path, index));
                }
                else
                {
                    left_ok = true;
                    ++result.left_count;
                }
            }
        }
        if (right_exists && !single_camera)
        {
            if (!cv::haveImageReader(right_path.string()))
            {
                result.issues.push_back(issue("unreadable_right_image",
                                              "right image is unreadable for pattern index " + std::to_string(index),
                                              right_path, index));
            }
            else
            {
                right = cv::imread(right_path.string(), cv::IMREAD_UNCHANGED);
                if (right.empty())
                {
                    result.issues.push_back(issue("empty_right_image",
                                                  "right image is empty for pattern index " + std::to_string(index),
                                                  right_path, index));
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
            result.issues.push_back(issue("stereo_size_mismatch",
                                          "left/right image sizes differ for pattern index " + std::to_string(index),
                                          left_path, index));
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
                result.issues.push_back(
                    issue("image_size_inconsistent",
                          "image size differs from first readable image for pattern index " + std::to_string(index),
                          left_ok ? left_path : right_path, index));
            }
        }
    }
}

} // namespace service::scan_dataset
