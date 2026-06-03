#include "handler/calibration_command_handler.hpp"

#include "service/calibration_service.hpp"

#include <type_traits>

namespace handler::calibration
{

bool handle(runtime::AppContext& ctx, win::Window& target_window, const cmd::Command& command)
{
    bool handled = false;

    std::visit(
        [&](auto&& c)
        {
            using T = std::decay_t<decltype(c)>;

            if constexpr (std::is_same_v<T, cmd::CmdCalibClear>)
            {
                service::calibration::clear(ctx, target_window, c);
                handled = true;
            }
            else if constexpr (std::is_same_v<T, cmd::CmdCalibCapture>)
            {
                service::calibration::capture(ctx, target_window, c);
                handled = true;
            }
            else if constexpr (std::is_same_v<T, cmd::CmdCalibrate>)
            {
                service::calibration::calibrate(ctx, c);
                handled = true;
            }
        },
        command);

    return handled;
}

} // namespace handler::calibration
