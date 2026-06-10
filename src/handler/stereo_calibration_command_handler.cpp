#include "handler/stereo_calibration_command_handler.hpp"

#include "runtime/handler_context.hpp"
#include "service/stereo_calibration_service.hpp"

#include <type_traits>

namespace handler::stereo_calibration
{

bool handle(runtime::StereoCalibrationHandlerContext& ctx, const cmd::Command& command)
{
    bool handled = false;

    std::visit(
        [&](auto&& c)
        {
            using T = std::decay_t<decltype(c)>;

            if constexpr (std::is_same_v<T, cmd::CmdStereoCalibrate>)
            {
                service::stereo_calibration::calibrate(ctx, c);
                handled = true;
            }
        },
        command);

    return handled;
}

} // namespace handler::stereo_calibration
