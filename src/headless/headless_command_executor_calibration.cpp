#include "headless/headless_command_executor.hpp"

#include "command_result_adapters.hpp"
#include "reconstruction/reconstruction_service.hpp"
#include "calibration/calibration_service.hpp"
#include "calibration/stereo_calibration_service.hpp"
#include "calibration/camera_projector_calibration_service.hpp"

namespace headless
{
common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdCalibrate& command)
{
    const auto result = calib::calibrate(cameras_, calibrator_, command);
    return result_adapter::calibration(command.role, command, result);
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdDetectCalibrationCorners& command)
{
    const auto result = calib::detectCorners(cameras_, calibrator_, command);
    if (!result.ok) return common::failure(result.error->code, result.error->message);
    return common::success({{"role", result.role},
        {"found", result.found ? "true" : "false"},
        {"corner_count", std::to_string(result.corner_count)},
        {"expected_corner_count", std::to_string(result.expected_corner_count)},
        {"path", result.output_path.string()}});
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdStereoCalibrate& command)
{
    const auto result = calib::calibrate(
        cameras_, stereo_calibrator_, stereo_data_, command);
    return result_adapter::stereoCalibration(
        command.left_role, command.right_role, command, result);
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdValidateReconstruction& command)
{
    return result_adapter::reconstruction(reconstruction_service_.validate(
        {command.decode_dir, command.calibration_file,
         {command.config.max_epipolar_error_px, command.config.min_depth_mm, command.config.max_depth_mm}}));
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdReconstructPointCloud& command)
{
    return result_adapter::reconstruction(reconstruction_service_.reconstruct(
        {{command.decode_dir, command.calibration_file,
          {command.config.max_epipolar_error_px, command.config.min_depth_mm, command.config.max_depth_mm}},
         command.output_file, command.overwrite}));
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdCameraProjectorCalibrate& command)
{
    const auto result = camera_projector_calibration_service_.calibrate({
        command.observations_dir, command.camera_calibration_file, command.output_file,
        {command.board_corners_x, command.board_corners_y}, command.square_size_mm,
        command.max_mean_displacement_px, command.max_corner_displacement_px, command.overwrite});
    if (!result.ok) return common::failure(result.error_code, result.error);
    return common::success({{"output_file",result.output_file.string()},
        {"total_pose_count",std::to_string(result.total_pose_count)},
        {"accepted_pose_count",std::to_string(result.accepted_pose_count)},
        {"rejected_pose_count",std::to_string(result.rejected_pose_count)},
        {"projector_rms",std::to_string(result.projector_rms)},
        {"stereo_rms",std::to_string(result.stereo_rms)},
        {"projector_width",std::to_string(result.projector_width)},
        {"projector_height",std::to_string(result.projector_height)}});
}
} // namespace headless
