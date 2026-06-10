#include "handler/reconstruction_command_handler.hpp"

#include "runtime/handler_context.hpp"
#include "service/reconstruction_service.hpp"

#include <type_traits>

namespace handler::reconstruction
{

bool handle(runtime::ReconstructionHandlerContext& ctx, win::Window& target_window, const cmd::Command& command)
{
    bool handled = false;

    std::visit(
        [&](auto&& c)
        {
            using T = std::decay_t<decltype(c)>;

            if constexpr (std::is_same_v<T, cmd::CmdReconstruct>)
            {
                service::reconstruction::run(ctx, target_window, c);
                handled = true;
            }
        },
        command);

    return handled;
}

} // namespace handler::reconstruction
