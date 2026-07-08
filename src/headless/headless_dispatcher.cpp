#include "headless/headless_dispatcher.hpp"

#include "capture/capture_service.hpp"
#include "handler/capture_command_handler.hpp"
#include "runtime/handler_context.hpp"

namespace headless
{

HeadlessDispatcher::HeadlessDispatcher(capture::CaptureService& capture_service)
    : capture_service_(capture_service)
{
}

common::CommandResult HeadlessDispatcher::execute(const cmd::Command& command)
{
    runtime::CaptureHandlerContext handler_ctx{capture_service_};
    const auto result = handler::capture::handle(handler_ctx, command);
    if (result.handled)
    {
        return result;
    }

    return common::notHandled();
}

} // namespace headless
