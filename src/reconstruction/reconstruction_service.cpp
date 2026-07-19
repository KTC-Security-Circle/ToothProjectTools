#include "reconstruction/reconstruction_service.hpp"
#include "projector_index_memory.hpp"

#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <cmath>
#include <fstream>
#include <iomanip>
#include <new>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace reconstruction
{
namespace
{
constexpr double kRotationOrthogonalityTolerance = 1e-4;
constexpr double kRotationDeterminantTolerance = 1e-4;
constexpr double kBytesPerMib = 1024.0 * 1024.0;

struct Side
{
    cv::Mat x;
    cv::Mat y;
    cv::Mat mask;
};

struct ProjectorCandidateIndex
{
    // bucket iの候補はcandidates[offsets[i], offsets[i + 1])へ連続格納する。
    std::vector<std::size_t> offsets;
    std::vector<cv::Point2f> candidates;
};

struct Data
{
    Side left;
    Side right;
    cv::Mat K1;
    cv::Mat D1;
    cv::Mat K2;
    cv::Mat D2;
    cv::Mat R;
    cv::Mat T;
    int image_width{};
    int image_height{};
    int projector_width{};
    int projector_height{};
    std::vector<cv::Point3d> points;
};

void addIssue(ReconstructionValidationResult& result,
              std::string code,
              std::string message,
              const std::filesystem::path& path)
{
    result.issues.push_back({std::move(code), std::move(message), path});
}

bool readMatrix(const std::filesystem::path& path, const char* key, cv::Mat& matrix)
{
    cv::FileStorage storage(path.string(), cv::FileStorage::READ);
    if (!storage.isOpened())
    {
        return false;
    }
    storage[key] >> matrix;
    return !matrix.empty();
}

bool isValidFloatingMatrix(const cv::Mat& matrix, int rows, int columns)
{
    return !matrix.empty() && matrix.rows == rows && matrix.cols == columns &&
           matrix.channels() == 1 &&
           (matrix.depth() == CV_32F || matrix.depth() == CV_64F) &&
           cv::checkRange(matrix, true);
}

bool isValidDistortionCoefficients(const cv::Mat& coefficients)
{
    if (coefficients.empty() || coefficients.channels() != 1 ||
        (coefficients.depth() != CV_32F && coefficients.depth() != CV_64F) ||
        (coefficients.rows != 1 && coefficients.cols != 1))
    {
        return false;
    }

    switch (coefficients.total())
    {
    case 4:
    case 5:
    case 8:
    case 12:
    case 14:
        return cv::checkRange(coefficients, true);
    default:
        return false;
    }
}

bool isValidRotationMatrix(const cv::Mat& rotation)
{
    if (!isValidFloatingMatrix(rotation, 3, 3))
    {
        return false;
    }

    cv::Mat rotation64;
    rotation.convertTo(rotation64, CV_64F);
    const cv::Mat identity = cv::Mat::eye(3, 3, CV_64F);
    const cv::Mat gram = rotation64.t() * rotation64;
    const double orthogonality_error = cv::norm(gram - identity, cv::NORM_INF);
    const double determinant = cv::determinant(rotation64);

    return std::isfinite(orthogonality_error) && std::isfinite(determinant) &&
           orthogonality_error <= kRotationOrthogonalityTolerance &&
           std::abs(determinant - 1.0) <= kRotationDeterminantTolerance;
}

std::string projectorIndexMemoryWarningMessage(const detail::ProjectorIndexMemoryEstimate& estimate,
                                               int projector_width,
                                               int projector_height)
{
    std::ostringstream message;
    message << "projector candidate index may require approximately "
            << static_cast<unsigned long long>(
                   std::ceil(static_cast<double>(estimate.estimated_peak_bytes) / kBytesPerMib))
            << " MiB for projector size " << projector_width << 'x' << projector_height;
    return message.str();
}

bool validateProjectorIndexMemory(const ReconstructionInput& input,
                                  ReconstructionValidationResult& result,
                                  const Data& data,
                                  std::size_t maximum_candidate_count)
{
    const auto estimate = detail::estimateProjectorIndexMemory(data.projector_width,
                                                               data.projector_height,
                                                               maximum_candidate_count);
    const auto metadata_path = input.decode_dir / "metadata.json";
    if (!estimate)
    {
        addIssue(result,
                 "decode_result_invalid",
                 "projector dimensions require excessive index memory",
                 metadata_path);
        return false;
    }
    if (detail::isProjectorIndexMemoryHardLimitExceeded(*estimate))
    {
        addIssue(result,
                 "decode_result_invalid",
                 "projector dimensions require excessive index memory",
                 metadata_path);
        return false;
    }
    if (detail::isProjectorIndexMemoryWarningLevel(*estimate))
    {
        result.warnings.push_back({"projector_index_memory_usage_high",
                                   projectorIndexMemoryWarningMessage(*estimate,
                                                                      data.projector_width,
                                                                      data.projector_height)});
    }
    return true;
}

std::size_t projectorCoordinateIndex(int x, int y, int width)
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
           static_cast<std::size_t>(x);
}

bool isProjectorCoordinateInRange(int x, int y, const Data& data)
{
    return x >= 0 && x < data.projector_width && y >= 0 && y < data.projector_height;
}

ProjectorCandidateIndex buildProjectorCandidateIndex(const Data& data, std::size_t bucket_count)
{
    ProjectorCandidateIndex index;
    index.offsets.assign(bucket_count + 1, 0);

    for (int y = 0; y < data.image_height; ++y)
    {
        for (int x = 0; x < data.image_width; ++x)
        {
            if (!data.right.mask.at<uchar>(y, x))
            {
                continue;
            }
            const int projector_x = data.right.x.at<int>(y, x);
            const int projector_y = data.right.y.at<int>(y, x);
            if (isProjectorCoordinateInRange(projector_x, projector_y, data))
            {
                const auto bucket =
                    projectorCoordinateIndex(projector_x, projector_y, data.projector_width);
                ++index.offsets[bucket + 1];
            }
        }
    }

    for (std::size_t bucket = 0; bucket < bucket_count; ++bucket)
    {
        index.offsets[bucket + 1] += index.offsets[bucket];
    }
    index.candidates.resize(index.offsets.back());
    std::vector<std::size_t> cursors(index.offsets.begin(), index.offsets.end() - 1);

    for (int y = 0; y < data.image_height; ++y)
    {
        for (int x = 0; x < data.image_width; ++x)
        {
            if (!data.right.mask.at<uchar>(y, x))
            {
                continue;
            }
            const int projector_x = data.right.x.at<int>(y, x);
            const int projector_y = data.right.y.at<int>(y, x);
            if (isProjectorCoordinateInRange(projector_x, projector_y, data))
            {
                const auto bucket =
                    projectorCoordinateIndex(projector_x, projector_y, data.projector_width);
                index.candidates[cursors[bucket]++] = cv::Point2f(x, y);
            }
        }
    }
    return index;
}

bool isValidTranslationVector(const cv::Mat& translation)
{
    const bool vector_shape =
        (translation.rows == 3 && translation.cols == 1) ||
        (translation.rows == 1 && translation.cols == 3);
    return !translation.empty() && translation.channels() == 1 && vector_shape &&
           (translation.depth() == CV_32F || translation.depth() == CV_64F) &&
           cv::checkRange(translation, true);
}

bool loadDecodeResult(const ReconstructionInput& input,
                      ReconstructionValidationResult& result,
                      Data& data)
{
    const auto metadata_path = input.decode_dir / "metadata.json";
    try
    {
        cv::FileStorage metadata(metadata_path.string(), cv::FileStorage::READ);
        if (!metadata.isOpened())
        {
            addIssue(result,
                     "decode_result_invalid",
                     "failed to open decode metadata",
                     metadata_path);
            return false;
        }

        const auto version = metadata["version"];
        if (version.empty())
        {
            result.warnings.push_back(
                {"decode_version_missing", "metadata.version is missing; treated as legacy v0"});
        }
        else if (!version.isString() || static_cast<std::string>(version) != "0.1.0")
        {
            addIssue(result,
                     "decode_result_version_unsupported",
                     "unsupported decode metadata version",
                     metadata_path);
            return false;
        }

        data.image_width = static_cast<int>(metadata["image_width"]);
        data.image_height = static_cast<int>(metadata["image_height"]);
        data.projector_width = static_cast<int>(metadata["projector_width"]);
        data.projector_height = static_cast<int>(metadata["projector_height"]);
        if (data.image_width <= 0 || data.image_height <= 0 || data.projector_width <= 0 ||
            data.projector_height <= 0)
        {
            addIssue(result,
                     "decode_result_invalid",
                     "decode metadata dimensions must be positive",
                     metadata_path);
            return false;
        }

        const auto loadSide = [&](const char* name, Side& side) {
            const auto side_dir = input.decode_dir / name;
            return readMatrix(side_dir / "projector_x.yml", "projector_x", side.x) &&
                   readMatrix(side_dir / "projector_y.yml", "projector_y", side.y) &&
                   !(side.mask = cv::imread((side_dir / "valid_mask.png").string(),
                                           cv::IMREAD_GRAYSCALE))
                        .empty();
        };
        if (!loadSide("left", data.left) || !loadSide("right", data.right))
        {
            addIssue(result,
                     "decode_result_invalid",
                     "decode map or mask is missing or malformed",
                     input.decode_dir);
            return false;
        }

        const cv::Size expected_size(data.image_width, data.image_height);
        const auto validSide = [&](const Side& side) {
            return side.x.type() == CV_32SC1 && side.y.type() == CV_32SC1 &&
                   side.mask.type() == CV_8UC1 && side.x.size() == expected_size &&
                   side.y.size() == expected_size && side.mask.size() == expected_size;
        };
        if (!validSide(data.left) || !validSide(data.right))
        {
            addIssue(result,
                     "decoded_map_mismatch",
                     "decode map/mask dimensions or types do not match metadata",
                     input.decode_dir);
            return false;
        }
    }
    catch (const cv::Exception&)
    {
        addIssue(result,
                 "decode_result_invalid",
                 "failed to parse decode result",
                 metadata_path);
        return false;
    }
    catch (const std::exception&)
    {
        addIssue(result,
                 "decode_result_invalid",
                 "failed to parse decode result",
                 metadata_path);
        return false;
    }
    return true;
}

bool loadCalibration(const ReconstructionInput& input,
                     ReconstructionValidationResult& result,
                     Data& data,
                     int& calibration_width,
                     int& calibration_height,
                     bool& has_calibration_size)
{
    try
    {
        cv::FileStorage calibration(input.calibration_file.string(), cv::FileStorage::READ);
        if (!calibration.isOpened())
        {
            addIssue(result,
                     "calibration_file_invalid",
                     "failed to parse stereo calibration file",
                     input.calibration_file);
            return false;
        }
        calibration["K1"] >> data.K1;
        calibration["D1"] >> data.D1;
        calibration["K2"] >> data.K2;
        calibration["D2"] >> data.D2;
        calibration["R"] >> data.R;
        calibration["T"] >> data.T;

        const auto width = calibration["image_width"];
        const auto height = calibration["image_height"];
        const bool has_width = !width.empty();
        const bool has_height = !height.empty();
        if (has_width != has_height)
        {
            addIssue(result,
                     "calibration_file_invalid",
                     "image_width and image_height must both be present or both be absent",
                     input.calibration_file);
            return false;
        }
        has_calibration_size = has_width;
        if (has_calibration_size)
        {
            if (!width.isInt() || !height.isInt())
            {
                addIssue(result,
                         "calibration_file_invalid",
                         "calibration image dimensions must be positive integers",
                         input.calibration_file);
                return false;
            }
            calibration_width = static_cast<int>(width);
            calibration_height = static_cast<int>(height);
            if (calibration_width <= 0 || calibration_height <= 0)
            {
                addIssue(result,
                         "calibration_file_invalid",
                         "calibration image dimensions must be positive integers",
                         input.calibration_file);
                return false;
            }
        }
    }
    catch (const cv::Exception&)
    {
        addIssue(result,
                 "calibration_file_invalid",
                 "failed to parse stereo calibration file",
                 input.calibration_file);
        return false;
    }
    catch (const std::exception&)
    {
        addIssue(result,
                 "calibration_file_invalid",
                 "failed to parse stereo calibration file",
                 input.calibration_file);
        return false;
    }
    return true;
}

bool validateCalibration(const ReconstructionInput& input,
                         ReconstructionValidationResult& result,
                         Data& data,
                         int calibration_width,
                         int calibration_height,
                         bool has_calibration_size)
{
    if (!isValidFloatingMatrix(data.K1, 3, 3) ||
        !isValidFloatingMatrix(data.K2, 3, 3) ||
        !isValidRotationMatrix(data.R) ||
        !isValidDistortionCoefficients(data.D1) ||
        !isValidDistortionCoefficients(data.D2) ||
        !isValidTranslationVector(data.T))
    {
        addIssue(result,
                 "calibration_file_invalid",
                 "stereo calibration matrices are missing or invalid",
                 input.calibration_file);
        return false;
    }

    data.K1.convertTo(data.K1, CV_64F);
    data.K2.convertTo(data.K2, CV_64F);
    data.D1.convertTo(data.D1, CV_64F);
    data.D2.convertTo(data.D2, CV_64F);
    data.R.convertTo(data.R, CV_64F);
    data.T = data.T.reshape(1, 3);
    data.T.convertTo(data.T, CV_64F);

    if (std::abs(cv::determinant(data.K1)) < 1e-12 ||
        std::abs(cv::determinant(data.K2)) < 1e-12 || cv::norm(data.T) < 1e-9)
    {
        addIssue(result,
                 "calibration_file_invalid",
                 "camera matrix is singular or stereo baseline is zero",
                 input.calibration_file);
        return false;
    }

    if (!has_calibration_size)
    {
        result.warnings.push_back(
            {"calibration_image_size_unavailable",
             "calibration image size is unavailable; dimension check was skipped"});
    }
    else if (calibration_width != data.image_width || calibration_height != data.image_height)
    {
        addIssue(result,
                 "image_size_mismatch",
                 "decode image size does not match calibration image size",
                 input.calibration_file);
        return false;
    }
    return true;
}

bool loadInput(const ReconstructionInput& input,
               ReconstructionValidationResult& result,
               Data& data)
{
    std::error_code error;
    if (!std::filesystem::is_directory(input.decode_dir, error))
    {
        addIssue(result,
                 "decode_dir_not_found",
                 "decode directory does not exist",
                 input.decode_dir);
        return false;
    }
    if (!std::filesystem::is_regular_file(input.calibration_file, error))
    {
        addIssue(result,
                 "calibration_file_not_found",
                 "stereo calibration file does not exist",
                 input.calibration_file);
        return false;
    }
    if (!loadDecodeResult(input, result, data))
    {
        return false;
    }

    int calibration_width = 0;
    int calibration_height = 0;
    bool has_calibration_size = false;
    return loadCalibration(input,
                           result,
                           data,
                           calibration_width,
                           calibration_height,
                           has_calibration_size) &&
           validateCalibration(input,
                               result,
                               data,
                               calibration_width,
                               calibration_height,
                               has_calibration_size);
}

bool calculate(const ReconstructionInput& input,
               ReconstructionValidationResult& result,
               Data& data)
{
    result.left_valid_count = cv::countNonZero(data.left.mask);
    result.right_valid_count = cv::countNonZero(data.right.mask);
    if (!validateProjectorIndexMemory(input,
                                      result,
                                      data,
                                      static_cast<std::size_t>(result.right_valid_count)))
    {
        return false;
    }

    const auto estimate = detail::estimateProjectorIndexMemory(data.projector_width,
                                                               data.projector_height,
                                                               static_cast<std::size_t>(
                                                                   result.right_valid_count));
    if (!estimate)
    {
        addIssue(result,
                 "decode_result_invalid",
                 "projector dimensions require excessive index memory",
                 input.decode_dir / "metadata.json");
        return false;
    }

    ProjectorCandidateIndex right_index;
    try
    {
        right_index = buildProjectorCandidateIndex(data, estimate->bucket_count);
    }
    catch (const std::bad_alloc&)
    {
        addIssue(result,
                 "decode_result_invalid",
                 "failed to allocate projector candidate index",
                 input.decode_dir / "metadata.json");
        return false;
    }
    catch (const std::length_error&)
    {
        addIssue(result,
                 "decode_result_invalid",
                 "failed to allocate projector candidate index",
                 input.decode_dir / "metadata.json");
        return false;
    }

    cv::Mat P1 = cv::Mat::zeros(3, 4, CV_64F);
    data.K1.copyTo(P1(cv::Rect(0, 0, 3, 3)));
    cv::Mat rotation_translation;
    cv::hconcat(data.R, data.T, rotation_translation);
    const cv::Mat P2 = data.K2 * rotation_translation;
    const cv::Mat translation_cross =
        (cv::Mat_<double>(3, 3) << 0, -data.T.at<double>(2), data.T.at<double>(1),
         data.T.at<double>(2), 0, -data.T.at<double>(0), -data.T.at<double>(1),
         data.T.at<double>(0), 0);
    const cv::Mat fundamental = data.K2.inv().t() * translation_cross * data.R * data.K1.inv();

    for (int y = 0; y < data.image_height; ++y)
    {
        for (int x = 0; x < data.image_width; ++x)
        {
            if (!data.left.mask.at<uchar>(y, x))
            {
                continue;
            }
            const int projector_x = data.left.x.at<int>(y, x);
            const int projector_y = data.left.y.at<int>(y, x);
            if (!isProjectorCoordinateInRange(projector_x, projector_y, data))
            {
                continue;
            }
            const auto bucket =
                projectorCoordinateIndex(projector_x, projector_y, data.projector_width);
            const auto candidate_begin = right_index.offsets[bucket];
            const auto candidate_end = right_index.offsets[bucket + 1];
            if (candidate_begin == candidate_end)
            {
                continue;
            }
            result.diagnostics.exact_match_candidate_count += candidate_end - candidate_begin;

            std::vector<cv::Point2f> left_raw{{static_cast<float>(x), static_cast<float>(y)}};
            std::vector<cv::Point2f> left;
            cv::undistortPoints(left_raw, left, data.K1, data.D1, cv::noArray(), data.K1);
            const cv::Mat line = fundamental * cv::Mat(cv::Vec3d(left[0].x, left[0].y, 1));
            const double A = line.at<double>(0);
            const double B = line.at<double>(1);
            const double C = line.at<double>(2);
            const double denominator = std::hypot(A, B);
            double best_error = input.config.max_epipolar_error_px;
            std::optional<cv::Point2f> best_point;
            for (auto candidate = candidate_begin; candidate < candidate_end; ++candidate)
            {
                std::vector<cv::Point2f> right_raw{right_index.candidates[candidate]};
                std::vector<cv::Point2f> right;
                cv::undistortPoints(right_raw, right, data.K2, data.D2, cv::noArray(), data.K2);
                const double error = denominator
                    ? std::abs(A * right[0].x + B * right[0].y + C) / denominator
                    : INFINITY;
                if (error < best_error)
                {
                    best_error = error;
                    best_point = right[0];
                }
            }
            if (!best_point)
            {
                ++result.diagnostics.epipolar_rejected_count;
                continue;
            }
            ++result.diagnostics.valid_correspondence_count;

            cv::Mat homogeneous;
            cv::triangulatePoints(P1,
                                  P2,
                                  std::vector<cv::Point2f>{left[0]},
                                  std::vector<cv::Point2f>{*best_point},
                                  homogeneous);
            homogeneous.convertTo(homogeneous, CV_64F);
            const double w = homogeneous.at<double>(3);
            if (!std::isfinite(w) || std::abs(w) < 1e-12)
            {
                ++result.diagnostics.triangulation_rejected_count;
                continue;
            }
            const cv::Point3d point(homogeneous.at<double>(0) / w,
                                    homogeneous.at<double>(1) / w,
                                    homogeneous.at<double>(2) / w);
            const cv::Mat right_point =
                data.R * (cv::Mat_<double>(3, 1) << point.x, point.y, point.z) + data.T;
            if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
                !std::isfinite(point.z) || point.z <= 0 || right_point.at<double>(2) <= 0)
            {
                ++result.diagnostics.triangulation_rejected_count;
                continue;
            }
            if ((input.config.min_depth_mm && point.z < *input.config.min_depth_mm) ||
                (input.config.max_depth_mm && point.z > *input.config.max_depth_mm))
            {
                ++result.diagnostics.depth_rejected_count;
                continue;
            }
            data.points.push_back(point);
        }
    }
    return true;
}

ReconstructionValidationResult prepare(const ReconstructionInput& input, Data& data)
{
    ReconstructionValidationResult result;
    if (!(input.config.max_epipolar_error_px > 0) ||
        !std::isfinite(input.config.max_epipolar_error_px))
    {
        addIssue(result, "invalid_command", "max_epipolar_error_px must be positive and finite", {});
    }
    if (input.config.min_depth_mm && *input.config.min_depth_mm <= 0)
    {
        addIssue(result, "invalid_command", "min_depth_mm must be positive", {});
    }
    if (input.config.max_depth_mm && *input.config.max_depth_mm <= 0)
    {
        addIssue(result, "invalid_command", "max_depth_mm must be positive", {});
    }
    if (input.config.min_depth_mm && input.config.max_depth_mm &&
        *input.config.min_depth_mm >= *input.config.max_depth_mm)
    {
        addIssue(result, "invalid_command", "min_depth_mm must be less than max_depth_mm", {});
    }
    if (!result.issues.empty() || !loadInput(input, result, data))
    {
        return result;
    }

    result.image_width = data.image_width;
    result.image_height = data.image_height;
    result.projector_width = data.projector_width;
    result.projector_height = data.projector_height;
    if (!calculate(input, result, data))
    {
        return result;
    }
    result.reconstructable_point_count = data.points.size();
    if (!result.diagnostics.valid_correspondence_count)
    {
        addIssue(result,
                 "insufficient_valid_correspondence",
                 "no correspondence passed exact and epipolar matching",
                 input.decode_dir);
    }
    else if (data.points.empty())
    {
        addIssue(result,
                 "triangulation_failed",
                 "no finite positive-depth point could be triangulated",
                 input.decode_dir);
    }
    result.valid = result.issues.empty();
    return result;
}
} // namespace

ReconstructionValidationResult ReconstructionService::validate(const ReconstructionInput& input) const
{
    Data data;
    return prepare(input, data);
}

ReconstructionResult ReconstructionService::reconstruct(const ReconstructionRequest& request) const
{
    ReconstructionResult result;
    result.output_file = request.output_file;
    std::error_code error;
    const bool output_exists = std::filesystem::exists(request.output_file, error);
    if (!error && output_exists && !request.overwrite)
    {
        result.error = ReconstructionIssue{"output_file_exists",
                                           "output file already exists; set overwrite=true to replace it",
                                           request.output_file};
        return result;
    }

    Data data;
    const auto validation = prepare(request.input, data);
    result.warnings = validation.warnings;
    result.diagnostics = validation.diagnostics;
    if (!validation.valid)
    {
        result.error = validation.issues.front();
        return result;
    }

    try
    {
        if (!request.output_file.parent_path().empty())
        {
            std::filesystem::create_directories(request.output_file.parent_path());
        }
        auto temporary = request.output_file;
        temporary += ".tmp";
        std::ofstream output(temporary);
        output << "ply\nformat ascii 1.0\nelement vertex " << data.points.size()
               << "\nproperty double x\nproperty double y\nproperty double z\nend_header\n"
               << std::setprecision(12);
        for (const auto& point : data.points)
        {
            output << point.x << ' ' << point.y << ' ' << point.z << '\n';
        }
        output.close();
        if (!output)
        {
            throw std::runtime_error("write");
        }
        std::filesystem::rename(temporary, request.output_file);
    }
    catch (...)
    {
        result.error = ReconstructionIssue{
            "file_write_failed", "failed to write reconstruction output", request.output_file};
        return result;
    }

    result.ok = true;
    result.point_count = data.points.size();
    return result;
}
} // namespace reconstruction
