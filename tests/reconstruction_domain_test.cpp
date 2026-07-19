#include "reconstruction/reconstruction_service.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <limits>

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

void writeValidDecodeResult()
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
                          (cv::Mat_<double>(3, 1) << -10.0, 0.0, 0.0))
{
    const cv::Mat camera =
        (cv::Mat_<double>(3, 3) << 100.0, 0.0, 1.5, 0.0, 100.0, 0.5, 0.0, 0.0, 1.0);
    cv::FileStorage storage(calibration_file.string(), cv::FileStorage::WRITE);
    storage << "K1" << camera << "D1" << d1 << "K2" << camera << "D2" << d2
            << "R" << cv::Mat::eye(3, 3, CV_64F) << "T" << translation
            << "image_width" << 4 << "image_height" << 2;
}

reconstruction::ReconstructionInput input()
{
    return {decode_dir, calibration_file, {2.0, {}, {}}};
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
    writeValidDecodeResult();
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
    writeValidDecodeResult();
    testValidReconstruction();
    testSupportedDistortionCoefficientCounts();
    testInvalidDistortionCoefficients();
    testMalformedCalibrationFile();
    testMalformedDecodeMetadata();
    testInvalidTranslationVectors();
    fs::remove_all(root);
}
