#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
namespace reconstruction {
struct ReconstructionConfig { double max_epipolar_error_px{2.0}; std::optional<double> min_depth_mm; std::optional<double> max_depth_mm; };
struct ReconstructionInput { std::filesystem::path decode_dir; std::filesystem::path calibration_file; ReconstructionConfig config; };
struct ReconstructionRequest { ReconstructionInput input; std::filesystem::path output_file; bool overwrite{false}; };
struct ReconstructionIssue { std::string code; std::string message; std::filesystem::path path; };
struct ReconstructionWarning { std::string code; std::string message; };
struct ReconstructionDiagnostics { size_t exact_match_candidate_count{}, valid_correspondence_count{}, epipolar_rejected_count{}, triangulation_rejected_count{}, depth_rejected_count{}; };
struct ReconstructionValidationResult { bool valid{}; std::vector<ReconstructionIssue> issues; std::vector<ReconstructionWarning> warnings; int image_width{},image_height{},projector_width{},projector_height{},left_valid_count{},right_valid_count{}; size_t reconstructable_point_count{}; ReconstructionDiagnostics diagnostics; };
struct ReconstructionResult { bool ok{}; std::optional<ReconstructionIssue> error; std::vector<ReconstructionWarning> warnings; std::filesystem::path output_file; size_t point_count{}; ReconstructionDiagnostics diagnostics; };
/** @brief 保存済みdecode resultからleft camera座標系・単位mmの点群を生成する。 */
class ReconstructionService { public: ReconstructionValidationResult validate(const ReconstructionInput&) const; ReconstructionResult reconstruct(const ReconstructionRequest&) const; };
}
