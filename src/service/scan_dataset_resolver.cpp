#include "service/scan_dataset_resolver.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>

namespace service::scan_dataset
{
namespace
{

ScanDatasetIssue issue(std::string code, std::string message, const std::filesystem::path& path = {}, int pattern_index = -1)
{
    return ScanDatasetIssue{std::move(code), std::move(message), path.empty() ? std::string{} : path.string(), pattern_index};
}

std::filesystem::path normalizeDirectoryPath(std::filesystem::path path)
{
    path = path.lexically_normal();
    auto text = path.string();
    while (text.size() > 1 && (text.back() == '/' || text.back() == '\\'))
    {
        text.pop_back();
    }
    return std::filesystem::path{text}.lexically_normal();
}

bool isDirectory(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::exists(path, ec) && std::filesystem::is_directory(path, ec);
}

bool isRegularFile(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec);
}

std::optional<int> patternIndex(const std::filesystem::path& path)
{
    if (path.extension() != ".png")
    {
        return std::nullopt;
    }
    const auto stem = path.stem().string();
    constexpr std::string_view prefix{"pattern_"};
    if (stem.rfind(prefix, 0) != 0 || stem.size() == prefix.size())
    {
        return std::nullopt;
    }
    int value = 0;
    for (std::size_t i = prefix.size(); i < stem.size(); ++i)
    {
        const auto ch = static_cast<unsigned char>(stem[i]);
        if (!std::isdigit(ch))
        {
            return std::nullopt;
        }
        value = value * 10 + (stem[i] - '0');
    }
    return value;
}

int maxPatternIndexPlusOne(const std::filesystem::path& dir)
{
    if (!isDirectory(dir))
    {
        return 0;
    }
    int max_index = -1;
    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }
        if (const auto index = patternIndex(entry.path()))
        {
            max_index = std::max(max_index, *index);
        }
    }
    return max_index + 1;
}

} // namespace

int inferPatternCount(const std::filesystem::path& left_dir, const std::filesystem::path& right_dir)
{
    return std::max(maxPatternIndexPlusOne(left_dir), maxPatternIndexPlusOne(right_dir));
}

ScanDatasetResolveResult ScanDatasetResolver::resolve(const ScanDatasetInputSpec& spec) const
{
    ScanDatasetResolveResult result;
    auto& dataset = result.dataset;

    if (spec.input_dir && !spec.left_dir && !spec.right_dir)
    {
        std::error_code ec;
        const auto input = normalizeDirectoryPath(*spec.input_dir);
        if (!std::filesystem::exists(input, ec))
        {
            result.issues.push_back(issue("input_dir_not_found", "input_dir does not exist", input));
            return result;
        }
        if (!std::filesystem::is_directory(input, ec))
        {
            result.issues.push_back(issue("input_dir_not_directory", "input_dir is not a directory", input));
            return result;
        }
    }

    if (spec.left_dir)
    {
        dataset.left_dir = normalizeDirectoryPath(*spec.left_dir);
        dataset.root_dir = spec.input_dir ? normalizeDirectoryPath(*spec.input_dir) : dataset.left_dir.parent_path();
        dataset.right_dir = spec.right_dir ? normalizeDirectoryPath(*spec.right_dir) : std::filesystem::path{};
    }
    else if (spec.input_dir)
    {
        const auto input = normalizeDirectoryPath(*spec.input_dir);
        const auto name = input.filename().string();
        if (name == "left")
        {
            dataset.root_dir = input.parent_path();
            dataset.left_dir = input;
            dataset.right_dir = dataset.root_dir / "right";
        }
        else if (name == "right")
        {
            dataset.root_dir = input.parent_path();
            dataset.left_dir = dataset.root_dir / "left";
            dataset.right_dir = input;
        }
        else
        {
            dataset.root_dir = input;
            dataset.left_dir = input / "left";
            dataset.right_dir = input / "right";
        }
    }

    if (!dataset.right_dir.empty() && !isDirectory(dataset.right_dir))
    {
        // input_dirから解決したrightが存在しない場合は単眼datasetとして扱う。
        dataset.right_dir.clear();
    }
    if (dataset.left_dir.empty())
    {
        result.issues.push_back(issue("missing_field", "input_dir or left_dir/right_dir is required"));
        return result;
    }

    if (!spec.left_dir && spec.right_dir)
    {
        result.issues.push_back(issue("missing_field", "missing required field: left_dir"));
    }

    if (spec.metadata_file)
    {
        dataset.metadata_file = spec.metadata_file->lexically_normal();
    }
    else
    {
        const auto metadata = dataset.root_dir / "metadata.json";
        if (isRegularFile(metadata))
        {
            dataset.metadata_file = metadata;
        }
    }
    dataset.metadata_present = dataset.metadata_file && isRegularFile(*dataset.metadata_file);

    if (!isDirectory(dataset.left_dir))
    {
        result.issues.push_back(issue("left_dir_not_found", "left directory does not exist", dataset.left_dir));
    }
    if (!dataset.right_dir.empty() && !isDirectory(dataset.right_dir))
    {
        result.issues.push_back(issue("right_dir_not_found", "right directory does not exist", dataset.right_dir));
    }

    if (spec.pattern_count)
    {
        dataset.pattern_count = *spec.pattern_count;
    }
    else
    {
        dataset.pattern_count = inferPatternCount(dataset.left_dir, dataset.right_dir);
    }
    result.ok = result.issues.empty();
    return result;
}

} // namespace service::scan_dataset
