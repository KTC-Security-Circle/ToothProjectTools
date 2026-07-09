#include "cmd/commands.hpp"
#include "control/control_message.hpp"
#include "handler/camera_command_handler.hpp"
#include "headless/headless_command_mapper.hpp"
#include "runtime/handler_context.hpp"
#include "service/camera_service.hpp"
#include "video/camera_manager.hpp"

#include <cassert>
#include <string>
#include <variant>

namespace
{

control::ControlMessage messageWithId()
{
    control::ControlMessage message;
    message.id = "test";
    return message;
}

void testMapper()
{
    video::CameraManager cameras;
    service::camera::CameraService service{cameras};
    headless::HeadlessCommandMapper mapper{service};

    auto message = messageWithId();
    message.role = "left";
    auto result = mapper.mapOpenCamera(message);
    assert(!result.ok && result.error->code == "missing_field");

    message = messageWithId();
    message.camera_id = 0;
    result = mapper.mapOpenCamera(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.role = "left";
    message.camera_id = -1;
    result = mapper.mapOpenCamera(message);
    assert(!result.ok && result.error->code == "invalid_command");

    message.camera_id = 0;
    result = mapper.mapOpenCamera(message);
    assert(result.ok && std::holds_alternative<cmd::CmdOpenCamera>(*result.command));

    message = messageWithId();
    result = mapper.mapCloseCamera(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.role = "left";
    result = mapper.mapCloseCamera(message);
    assert(result.ok && std::holds_alternative<cmd::CmdCloseCamera>(*result.command));
}

void testHandler()
{
    video::CameraManager cameras;
    service::camera::CameraService service{cameras};
    runtime::CameraHandlerContext context{service};

    const auto open = handler::camera::handle(context, cmd::Command{cmd::CmdOpenCamera{0, ""}});
    assert(open.handled);
    const auto close = handler::camera::handle(context, cmd::Command{cmd::CmdCloseCamera{"left"}});
    assert(close.handled);
    const auto other = handler::camera::handle(context, cmd::Command{cmd::CmdCaptureFrame{}});
    assert(!other.handled);
}

void testServiceValidation()
{
    video::CameraManager cameras;
    service::camera::CameraService service{cameras};
    assert(!service.resolveCameraId("left"));
    const auto invalid = service.openCamera(0, "");
    assert(!invalid.ok && invalid.error->code == "invalid_command");
    const auto close = service.closeCamera("left");
    assert(!close.ok && close.error->code == "camera_not_open");
}

} // namespace

int main()
{
    testMapper();
    testHandler();
    testServiceValidation();
    return 0;
}
