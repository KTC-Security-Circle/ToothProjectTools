#include "handler/stereo_calibration_command_handler.hpp"

#include "command_result_mapper/stereo_calibration_command_result_mapper.hpp"
#include "runtime/handler_context.hpp"
#include "service/stereo_calibration_service.hpp"

#include <type_traits>

namespace handler::stereo_calibration
{

common::CommandResult handle(runtime::StereoCalibrationCalcContext& ctx, const cmd::Command& command)
{
    return std::visit(
        [&](auto&& c) -> common::CommandResult
        {
            using T = std::decay_t<decltype(c)>;

            if constexpr (std::is_same_v<T, cmd::CmdStereoCalibrate>)
            {
                const auto result = service::stereo_calibration::calibrate(ctx, c);
                return command_result_mapper::stereo_calibration::toCommandResult(c.left_role, c.right_role, c, result);
            }
            else
            {
                return common::notHandled();
            }
        },
        command);
}

} // namespace handler::stereo_calibration
