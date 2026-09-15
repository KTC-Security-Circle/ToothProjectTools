#include "headless/headless_command_executor.hpp"

#include "command_result_mapper/calibration_command_result_mapper.hpp"
#include "command_result_mapper/reconstruction_command_result_mapper.hpp"
#include "command_result_mapper/stereo_calibration_command_result_mapper.hpp"
#include "reconstruction/reconstruction_service.hpp"
#include "service/calibration_service.hpp"
#include "service/stereo_calibration_service.hpp"

namespace headless
{
common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdCalibrate& command)
{
    const auto result = service::calibration::calibrate(cameras_, calibrator_, command);
    return command_result_mapper::calibration::toCommandResult(command.role, command, result);
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdStereoCalibrate& command)
{
    const auto result = service::stereo_calibration::calibrate(
        cameras_, stereo_calibrator_, stereo_data_, command);
    return command_result_mapper::stereo_calibration::toCommandResult(
        command.left_role, command.right_role, command, result);
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdValidateReconstruction& command)
{
    return command_result_mapper::reconstruction::toCommandResult(reconstruction_service_.validate(
        {command.decode_dir, command.calibration_file,
         {command.config.max_epipolar_error_px, command.config.min_depth_mm, command.config.max_depth_mm}}));
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdReconstructPointCloud& command)
{
    return command_result_mapper::reconstruction::toCommandResult(reconstruction_service_.reconstruct(
        {{command.decode_dir, command.calibration_file,
          {command.config.max_epipolar_error_px, command.config.min_depth_mm, command.config.max_depth_mm}},
         command.output_file, command.overwrite}));
}
} // namespace headless
