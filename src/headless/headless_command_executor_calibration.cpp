#include "headless/headless_command_executor.hpp"

#include "command_result_adapters.hpp"
#include "reconstruction/reconstruction_service.hpp"
#include "calibration/calibration_service.hpp"
#include "calibration/stereo_calibration_service.hpp"

namespace headless
{
common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdCalibrate& command)
{
    const auto result = calib::calibrate(cameras_, calibrator_, command);
    return result_adapter::calibration(command.role, command, result);
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
} // namespace headless
