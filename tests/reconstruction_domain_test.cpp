#include "reconstruction/reconstruction_service.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>

namespace
{
namespace fs = std::filesystem;

void require(bool condition, const char* expression, int line)
{
    if (!condition)
    {
        std::cerr << "requirement failed at line " << line << ": " << expression << '\n';
        std::abort();
    }
}

#define REQUIRE(expression) require(static_cast<bool>(expression), #expression, __LINE__)
const fs::path root = fs::temp_directory_path() / "tooth_reconstruction_test";
const fs::path decode_dir = root / "decode";
const fs::path calibration_file = root / "stereo.yml";

void writeMatrix(const fs::path& path, const char* key, const cv::Mat& matrix)
{
    cv::FileStorage storage(path.string(), cv::FileStorage::WRITE);
    storage << key << matrix;
}

void writeDecodeResult(bool multiple_candidates = false, bool multiple_buckets = false,
                       bool out_of_range_candidates = false)
{
    fs::create_directories(decode_dir / "left");
    fs::create_directories(decode_dir / "right");
    std::ofstream(decode_dir / "metadata.json")
        << "{\"version\":\"0.1.0\",\"image_width\":4,\"image_height\":2,"
           "\"projector_width\":8,\"projector_height\":4}";
    cv::Mat lx(2, 4, CV_32S, cv::Scalar(-1));
    cv::Mat ly(2, 4, CV_32S, cv::Scalar(-1));
    cv::Mat rx = lx.clone();
    cv::Mat ry = ly.clone();
    cv::Mat lm(2, 4, CV_8U, cv::Scalar(0));
    cv::Mat rm(2, 4, CV_8U, cv::Scalar(0));
    lx.at<int>(0, 2) = ly.at<int>(0, 2) = 1;
    rx.at<int>(0, 1) = ry.at<int>(0, 1) = 1;
    lm.at<uchar>(0, 2) = 255;
    rm.at<uchar>(0, 1) = 255;
    if (multiple_candidates)
    {
        rx.at<int>(1, 0) = 1;
        ry.at<int>(1, 0) = 1;
        rm.at<uchar>(1, 0) = 255;
    }
    if (multiple_buckets)
    {
        lx.at<int>(0, 3) = 2;
        ly.at<int>(0, 3) = 1;
        lm.at<uchar>(0, 3) = 255;
        rx.at<int>(0, 2) = 2;
        ry.at<int>(0, 2) = 1;
        rm.at<uchar>(0, 2) = 255;
    }
    if (out_of_range_candidates)
    {
        rx.at<int>(1, 0) = -1;
        ry.at<int>(1, 0) = 1;
        rx.at<int>(1, 1) = 1;
        ry.at<int>(1, 1) = -1;
        rx.at<int>(1, 2) = 8;
        ry.at<int>(1, 2) = 1;
        rx.at<int>(1, 3) = 1;
        ry.at<int>(1, 3) = 4;
        rm.row(1).setTo(255);
    }
    writeMatrix(decode_dir / "left/projector_x.yml", "projector_x", lx);
    writeMatrix(decode_dir / "left/projector_y.yml", "projector_y", ly);
    writeMatrix(decode_dir / "right/projector_x.yml", "projector_x", rx);
    writeMatrix(decode_dir / "right/projector_y.yml", "projector_y", ry);
    REQUIRE(cv::imwrite((decode_dir / "left/valid_mask.png").string(), lm));
    REQUIRE(cv::imwrite((decode_dir / "right/valid_mask.png").string(), rm));
}

void writeCalibration(const cv::Mat& d1,
                      const cv::Mat& d2 = cv::Mat::zeros(1, 5, CV_64F),
                      const cv::Mat& translation =
                          (cv::Mat_<double>(3, 1) << -10.0, 0.0, 0.0),
                      std::optional<int> image_width = 4,
                      std::optional<int> image_height = 2)
{
    const cv::Mat camera =
        (cv::Mat_<double>(3, 3) << 100.0, 0.0, 1.5, 0.0, 100.0, 0.5, 0.0, 0.0, 1.0);
    cv::FileStorage storage(calibration_file.string(), cv::FileStorage::WRITE);
    storage << "K1" << camera << "D1" << d1 << "K2" << camera << "D2" << d2
            << "R" << cv::Mat::eye(3, 3, CV_64F) << "T" << translation;
    if (image_width)
    {
        storage << "image_width" << *image_width;
    }
    if (image_height)
    {
        storage << "image_height" << *image_height;
    }
}

void writeCalibrationWithStringWidth()
{
    const cv::Mat camera =
        (cv::Mat_<double>(3, 3) << 100.0, 0.0, 1.5, 0.0, 100.0, 0.5, 0.0, 0.0, 1.0);
    const cv::Mat distortion = cv::Mat::zeros(1, 5, CV_64F);
    const cv::Mat translation = (cv::Mat_<double>(3, 1) << -10.0, 0.0, 0.0);
    cv::FileStorage storage(calibration_file.string(), cv::FileStorage::WRITE);
    storage << "K1" << camera << "D1" << distortion << "K2" << camera << "D2" << distortion
            << "R" << cv::Mat::eye(3, 3, CV_64F) << "T" << translation
            << "image_width" << "4" << "image_height" << 2;
}

reconstruction::ReconstructionInput input(double max_epipolar_error_px = 2.0)
{
    return {decode_dir, calibration_file, {max_epipolar_error_px, {}, {}}};
}

reconstruction::ReconstructionValidationResult validateWithoutThrow()
{
    reconstruction::ReconstructionValidationResult result;
    bool threw = false;
    try
    {
        result = reconstruction::ReconstructionService{}.validate(input());
    }
    catch (...)
    {
        threw = true;
    }
    REQUIRE(!threw);
    return result;
}

void assertCalibrationInvalid(const cv::Mat& d1)
{
    writeCalibration(d1);
    const auto result = validateWithoutThrow();
    REQUIRE(!result.valid && !result.issues.empty());
    REQUIRE(result.issues.front().code == "calibration_file_invalid");
    REQUIRE(result.issues.front().path == calibration_file);
}

void testValidReconstruction()
{
    writeCalibration(cv::Mat::zeros(1, 5, CV_64F));
    reconstruction::ReconstructionService service;
    const auto validation = service.validate(input());
    REQUIRE(validation.valid && validation.reconstructable_point_count == 1);
    const auto output = root / "cloud.ply";
    const auto result = service.reconstruct({input(), output, false});
    REQUIRE(result.ok && result.point_count == 1 && fs::exists(output));
    const auto conflict = service.reconstruct({input(), output, false});
    REQUIRE(!conflict.ok && conflict.error->code == "output_file_exists");
}

void testOutputConflictReturnsBeforeInputValidation()
{
    const auto output = root / "existing-output.ply";
    std::ofstream(output) << "existing";
    reconstruction::ReconstructionInput invalid_input{
        root / "missing-decode", root / "missing-calibration.yml", {2.0, {}, {}}};
    const auto result =
        reconstruction::ReconstructionService{}.reconstruct({invalid_input, output, false});
    REQUIRE(!result.ok && result.error.has_value());
    REQUIRE(result.error->code == "output_file_exists");
    REQUIRE(result.error->path == output);
    REQUIRE(result.diagnostics.exact_match_candidate_count == 0);
}

void testCalibrationImageSizeMetadata()
{
    const cv::Mat distortion = cv::Mat::zeros(1, 5, CV_64F);
    const cv::Mat translation = (cv::Mat_<double>(3, 1) << -10.0, 0.0, 0.0);

    writeCalibration(distortion, distortion, translation, 4, std::nullopt);
    auto result = validateWithoutThrow();
    REQUIRE(!result.valid && result.issues.front().code == "calibration_file_invalid");
    REQUIRE(result.issues.front().path == calibration_file);

    writeCalibration(distortion, distortion, translation, std::nullopt, 2);
    result = validateWithoutThrow();
    REQUIRE(!result.valid && result.issues.front().code == "calibration_file_invalid");

    writeCalibration(distortion, distortion, translation, std::nullopt, std::nullopt);
    result = validateWithoutThrow();
    REQUIRE(result.valid);
    bool found_legacy_warning = false;
    for (const auto& warning : result.warnings)
    {
        found_legacy_warning |= warning.code == "calibration_image_size_unavailable";
    }
    REQUIRE(found_legacy_warning);

    writeCalibration(distortion, distortion, translation, 4, 2);
    REQUIRE(validateWithoutThrow().valid);

    writeCalibration(distortion, distortion, translation, 5, 2);
    result = validateWithoutThrow();
    REQUIRE(!result.valid && result.issues.front().code == "image_size_mismatch");
    REQUIRE(result.issues.front().path == calibration_file);

    writeCalibration(distortion, distortion, translation, 0, 2);
    result = validateWithoutThrow();
    REQUIRE(!result.valid && result.issues.front().code == "calibration_file_invalid");

    writeCalibration(distortion, distortion, translation, 4, -1);
    result = validateWithoutThrow();
    REQUIRE(!result.valid && result.issues.front().code == "calibration_file_invalid");

    writeCalibrationWithStringWidth();
    result = validateWithoutThrow();
    REQUIRE(!result.valid && result.issues.front().code == "calibration_file_invalid");
}

void testProjectorCandidateBuckets()
{
    writeCalibration(cv::Mat::zeros(1, 5, CV_64F));

    writeDecodeResult(true, false, false);
    auto result = reconstruction::ReconstructionService{}.validate(input(0.5));
    REQUIRE(result.valid);
    REQUIRE(result.diagnostics.exact_match_candidate_count == 2);
    REQUIRE(result.diagnostics.valid_correspondence_count == 1);
    REQUIRE(result.reconstructable_point_count == 1);
    REQUIRE(result.diagnostics.epipolar_rejected_count == 0);
    REQUIRE(result.diagnostics.triangulation_rejected_count == 0);
    REQUIRE(result.diagnostics.depth_rejected_count == 0);

    writeDecodeResult(false, true, false);
    result = validateWithoutThrow();
    REQUIRE(result.valid);
    REQUIRE(result.diagnostics.exact_match_candidate_count == 2);
    REQUIRE(result.diagnostics.valid_correspondence_count == 2);
    REQUIRE(result.reconstructable_point_count == 2);

    writeDecodeResult(false, false, true);
    result = validateWithoutThrow();
    REQUIRE(result.valid);
    REQUIRE(result.diagnostics.exact_match_candidate_count == 1);
    REQUIRE(result.diagnostics.valid_correspondence_count == 1);
    REQUIRE(result.reconstructable_point_count == 1);
    writeDecodeResult();
}

void testSupportedDistortionCoefficientCounts()
{
    for (const int count : {4, 5, 8, 12, 14})
    {
        writeCalibration(cv::Mat::zeros(1, count, CV_64F));
        REQUIRE(reconstruction::ReconstructionService{}.validate(input()).valid);
    }
}

void testInvalidDistortionCoefficients()
{
    assertCalibrationInvalid(cv::Mat::zeros(2, 2, CV_64F));
    assertCalibrationInvalid(cv::Mat::zeros(1, 5, CV_64FC2));
    assertCalibrationInvalid(cv::Mat::zeros(1, 6, CV_64F));
    assertCalibrationInvalid(cv::Mat::zeros(1, 5, CV_32S));
    cv::Mat nan = cv::Mat::zeros(1, 5, CV_64F);
    nan.at<double>(0, 2) = std::numeric_limits<double>::quiet_NaN();
    assertCalibrationInvalid(nan);
    const cv::Mat valid = cv::Mat::zeros(1, 5, CV_64F);
    writeCalibration(valid, cv::Mat::zeros(2, 2, CV_64F));
    const auto d2_result = validateWithoutThrow();
    REQUIRE(!d2_result.valid && d2_result.issues.front().code == "calibration_file_invalid");
}

void testMalformedCalibrationFile()
{
    std::ofstream(calibration_file) << "%YAML:1.0\nK1: [";
    const auto result = validateWithoutThrow();
    REQUIRE(!result.valid && result.issues.front().code == "calibration_file_invalid");
    REQUIRE(result.issues.front().code != "decode_result_invalid");
    REQUIRE(result.issues.front().path == calibration_file);
}

void testMalformedDecodeMetadata()
{
    writeCalibration(cv::Mat::zeros(1, 5, CV_64F));
    std::ofstream(decode_dir / "metadata.json") << "{\"version\":";
    const auto result = validateWithoutThrow();
    REQUIRE(!result.valid && result.issues.front().code == "decode_result_invalid");
    REQUIRE(result.issues.front().code != "calibration_file_invalid");
    REQUIRE(result.issues.front().path == decode_dir / "metadata.json");
    writeDecodeResult();
}

void testInvalidTranslationVectors()
{
    const cv::Mat d = cv::Mat::zeros(1, 5, CV_64F);
    for (const cv::Mat t : {cv::Mat::zeros(2, 2, CV_64F),
                             cv::Mat::zeros(3, 1, CV_32S),
                             cv::Mat::zeros(3, 1, CV_64F)})
    {
        writeCalibration(d, d, t);
        const auto result = validateWithoutThrow();
        REQUIRE(!result.valid && result.issues.front().code == "calibration_file_invalid");
        REQUIRE(result.issues.front().path == calibration_file);
    }
    cv::Mat inf = (cv::Mat_<double>(3, 1) << -10.0, 0.0, 0.0);
    inf.at<double>(1) = std::numeric_limits<double>::infinity();
    writeCalibration(d, d, inf);
    const auto result = validateWithoutThrow();
    REQUIRE(!result.valid && result.issues.front().code == "calibration_file_invalid");
}
} // namespace

int main()
{
    fs::remove_all(root);
    writeDecodeResult();
    testValidReconstruction();
    testOutputConflictReturnsBeforeInputValidation();
    testCalibrationImageSizeMetadata();
    testProjectorCandidateBuckets();
    testSupportedDistortionCoefficientCounts();
    testInvalidDistortionCoefficients();
    testMalformedCalibrationFile();
    testMalformedDecodeMetadata();
    testInvalidTranslationVectors();
    fs::remove_all(root);
}
