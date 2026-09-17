#include "calibration/camera_projector_calibration_service.hpp"

#include "calibration/atomic_calibration_file.hpp"
#include "calibration/calibration_file.hpp"
#include "calibration/projector_calibrator.hpp"
#include "scan/scan_dataset_validator.hpp"

#include <algorithm>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/core/persistence.hpp>

namespace calib::projector
{
namespace
{
ServiceResult failure(const CalibrationConfig& config, std::string code, std::string message)
{
    ServiceResult result;
    result.output_file = config.output_file;
    result.error_code = std::move(code);
    result.error = std::move(message);
    return result;
}

bool loadMap(const std::filesystem::path& path, const char* key, cv::Mat& value)
{
    try { cv::FileStorage storage(path.string(), cv::FileStorage::READ); if (!storage.isOpened()) return false;
          storage[key] >> value; return !value.empty(); }
    catch (const cv::Exception&) { return false; }
}

bool supportedMap(const cv::Mat& value)
{
    return value.channels() == 1 && (value.type() == CV_32S || value.type() == CV_32F);
}

bool sameSurface(const scan::dataset::ScanDatasetMetadata& a, const scan::dataset::ScanDatasetMetadata& b)
{
    return a.projector_width == b.projector_width && a.projector_height == b.projector_height &&
           a.pattern_x == b.pattern_x && a.pattern_y == b.pattern_y &&
           a.pattern_width == b.pattern_width && a.pattern_height == b.pattern_height;
}
}

CameraProjectorCalibrationService::CameraProjectorCalibrationService(
    const scan::dataset::ScanDatasetValidator& validator) : validator_(validator) {}

ServiceResult CameraProjectorCalibrationService::calibrate(const CalibrationConfig& config) const
{
    if (config.board_size.width <= 0 || config.board_size.height <= 0 || config.square_size_mm <= 0.0 ||
        config.max_mean_displacement_px < 0.0 || config.max_corner_displacement_px < 0.0 ||
        config.observations_dir.empty() || config.camera_calibration_file.empty() || config.output_file.empty())
        return failure(config, "camera_projector_invalid_config", "invalid Camera-Projector calibration config");
    std::error_code ec;
    if (!std::filesystem::is_directory(config.observations_dir, ec))
        return failure(config, "camera_projector_observations_invalid", "observations_dir is not a directory");
    if (std::filesystem::exists(config.output_file, ec) && !config.overwrite)
        return failure(config, "camera_projector_output_exists", "output_file already exists");

    std::string mono_error;
    const auto mono = calib::file::loadMonoCalibrationFile(config.camera_calibration_file, mono_error);
    if (!mono) return failure(config, "camera_projector_mono_load_failed", mono_error);

    std::vector<std::filesystem::path> pose_dirs;
    for (const auto& entry : std::filesystem::directory_iterator(config.observations_dir, ec))
        if (entry.is_directory() && !entry.path().filename().string().starts_with('.')) pose_dirs.push_back(entry.path());
    if (ec) return failure(config, "camera_projector_observations_invalid", ec.message());
    std::sort(pose_dirs.begin(), pose_dirs.end());

    ServiceResult result;
    result.output_file = config.output_file;
    result.total_pose_count = static_cast<int>(pose_dirs.size());
    std::vector<CalibrationObservation> observations;
    std::optional<scan::dataset::ScanDatasetMetadata> common_metadata;
    cv::Size camera_size;
    bool artifact_failure = false;
    for (const auto& pose_dir : pose_dirs)
    {
        PoseDiagnostic diagnostic;
        diagnostic.pose_name = pose_dir.filename().string();
        std::vector<scan::dataset::ScanDatasetIssue> issues;
        const auto scan_metadata = validator_.readMetadataForDecode(pose_dir / "scan", issues);
        const auto decode_metadata = validator_.readMetadataForDecode(pose_dir / "decode", issues);
        if (scan_metadata && decode_metadata && !sameSurface(*scan_metadata, *decode_metadata))
            return failure(config, "camera_projector_inconsistent_surface", "scan and decode projector surfaces differ");
        if (!scan_metadata || !decode_metadata || !issues.empty())
        {
            diagnostic.reason = "scan/decode metadata is missing, invalid, or inconsistent";
            artifact_failure = true; result.poses.push_back(std::move(diagnostic)); continue;
        }
        if (!common_metadata) common_metadata = *scan_metadata;
        else if (!sameSurface(*common_metadata, *scan_metadata))
            return failure(config, "camera_projector_inconsistent_surface", "projector resolution or pattern surface differs between poses");
        if (scan_metadata->projector_width != 480 || scan_metadata->projector_height != 270)
            return failure(config, "camera_projector_invalid_config", "only 480x270 logical projector resolution is supported");

        const auto before = cv::imread((pose_dir / "reference_before.png").string(), cv::IMREAD_UNCHANGED);
        const auto after = cv::imread((pose_dir / "reference_after.png").string(), cv::IMREAD_UNCHANGED);
        cv::Mat projector_x, projector_y;
        const auto decode_left = pose_dir / "decode" / "left";
        const auto mask = cv::imread((decode_left / "valid_mask.png").string(), cv::IMREAD_UNCHANGED);
        if (before.empty() || after.empty() || before.size() != after.size() ||
            !loadMap(decode_left / "projector_x.yml", "projector_x", projector_x) ||
            !loadMap(decode_left / "projector_y.yml", "projector_y", projector_y) ||
            !supportedMap(projector_x) || !supportedMap(projector_y) || mask.empty() ||
            mask.type() != CV_8UC1 || projector_x.size() != before.size() || projector_y.size() != before.size() ||
            mask.size() != before.size())
        {
            diagnostic.reason = "observation artifact is missing or malformed";
            artifact_failure = true; result.poses.push_back(std::move(diagnostic)); continue;
        }
        if (camera_size.empty()) camera_size = before.size();
        if (camera_size != before.size() || (mono->image_width > 0 &&
            camera_size != cv::Size{mono->image_width, mono->image_height}))
            return failure(config, "camera_projector_artifact_load_failed", "camera image sizes are inconsistent");

        const auto observation = makeObservation(before, after, projector_x, projector_y, mask, config.board_size,
                                                 config.square_size_mm, config.max_mean_displacement_px,
                                                 config.max_corner_displacement_px);
        diagnostic.accepted = observation.valid;
        diagnostic.reason = observation.error;
        diagnostic.mean_corner_displacement_px = observation.mean_corner_displacement;
        diagnostic.max_corner_displacement_px = observation.max_corner_displacement;
        if (observation.valid) observations.push_back(observation.observation);
        result.poses.push_back(std::move(diagnostic));
    }
    result.accepted_pose_count = static_cast<int>(observations.size());
    result.rejected_pose_count = result.total_pose_count - result.accepted_pose_count;
    if (observations.size() < 3)
    {
        result.error_code = artifact_failure && observations.empty() ? "camera_projector_artifact_load_failed"
                                                                     : "camera_projector_insufficient_poses";
        result.error = "fewer than three valid poses";
        return result;
    }
    const cv::Size projector_size{common_metadata->projector_width, common_metadata->projector_height};
    result.projector_width = projector_size.width; result.projector_height = projector_size.height;
    const auto solved = calib::projector::calibrate(observations, camera_size, projector_size, mono->K, mono->D,
                                                    config.square_size_mm);
    if (!solved.ok) { result.error_code = "camera_projector_solve_failed"; result.error = solved.error; return result; }

    const auto parent = config.output_file.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);
    if (ec) return failure(config, "camera_projector_output_write_failed", ec.message());
    const auto temporary = calib::file::createTemporaryCalibrationPath(config.output_file);
    if (!temporary) return failure(config, "camera_projector_output_write_failed", "failed to create temporary output");
    std::string save_error;
    const ProjectorSurface surface{common_metadata->pattern_x, common_metadata->pattern_y,
                                   common_metadata->pattern_width, common_metadata->pattern_height};
    if (!saveCalibration(*temporary, camera_size, projector_size, surface, config.board_size, mono->K, mono->D,
                         solved, config.square_size_mm, save_error))
    {
        std::filesystem::remove(*temporary, ec);
        return failure(config, "camera_projector_output_write_failed", save_error);
    }
    ec.clear(); std::filesystem::rename(*temporary, config.output_file, ec);
    if (ec) { std::filesystem::remove(*temporary, ec); return failure(config, "camera_projector_output_write_failed", "failed to replace output file"); }
    result.ok = true; result.projector_rms = solved.projector_rms; result.stereo_rms = solved.stereo_rms;
    return result;
}
} // namespace calib::projector
