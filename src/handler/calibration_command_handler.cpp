#include "handler/calibration_command_handler.hpp"

#include "command_result_mapper/calibration_command_result_mapper.hpp"
#include "runtime/handler_context.hpp"
#include "service/calibration_service.hpp"

#include <type_traits>

namespace handler::calibration
{

common::CommandResult handle(runtime::MonoCalibrationCalcContext& ctx, const cmd::Command& command)
{
    return std::visit(
        [&](auto&& c) -> common::CommandResult
        {
            using T = std::decay_t<decltype(c)>;

            if constexpr (std::is_same_v<T, cmd::CmdCalibrate>)
            {
                const auto result = service::calibration::calibrate(ctx, c);
                return command_result_mapper::calibration::toCommandResult(c.role, c, result);
            }
            else
            {
                return common::notHandled();
            }
        },
        command);
}

} // namespace handler::calibration
