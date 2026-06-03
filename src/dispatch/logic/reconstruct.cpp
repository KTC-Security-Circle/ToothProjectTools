#include "dispatch/logic/reconstruct.hpp"

#include "service/reconstruction_service.hpp"

namespace dispatch::logic
{

void run_reconstruction(runtime::AppContext& ctx, const cmd::CmdReconstruct& c, win::Window& target_window)
{
    service::reconstruction::run(ctx, target_window, c);
}

} // namespace dispatch::logic
