#include "headless/headless_dispatcher.hpp"

#include "capture/capture_service.hpp"
#include "handler/calibration_command_handler.hpp"
#include "handler/camera_command_handler.hpp"
#include "handler/capture_command_handler.hpp"
#include "handler/projector_command_handler.hpp"
#include "handler/scan_command_handler.hpp"
#include "handler/scan_dataset_command_handler.hpp"
#include "handler/window_resource_command_handler.hpp"
#include "handler/stereo_calibration_command_handler.hpp"
#include "runtime/handler_context.hpp"
#include "service/scan_service.hpp"
#include "service/scan_dataset_validator.hpp"

#include <optional>
#include <string>
#include <type_traits>
#include <variant>

namespace headless
{
namespace
{

std::optional<common::CommandResult> busyProjectorResult(const service::scan::ScanService& scan_service,
                                                          const std::string& projector_role)
{
    if (!scan_service.isProjectorRoleBusy(projector_role))
    {
        return std::nullopt;
    }
    return common::failure("scan_resource_busy", "projector role is used by active scan: " + projector_role);
}

std::optional<common::CommandResult> busyCameraResult(const service::scan::ScanService& scan_service,
                                                       const std::string& camera_role)
{
    if (!scan_service.isCameraRoleBusy(camera_role))
    {
        return std::nullopt;
    }
    return common::failure("scan_resource_busy", "camera role is used by active scan: " + camera_role);
}

std::optional<common::CommandResult> busyWindowResult(const service::scan::ScanService& scan_service,
                                                       const std::string& window_role)
{
    if (!scan_service.isWindowRoleBusy(window_role))
    {
        return std::nullopt;
    }
    return common::failure("scan_resource_busy", "window role is used by active scan: " + window_role);
}

std::optional<common::CommandResult> busyCaptureResult(const service::scan::ScanService& scan_service)
{
    if (!scan_service.isScanActive())
    {
        return std::nullopt;
    }
    return common::failure("scan_resource_busy", "capture command conflicts with active scan");
}

} // namespace

HeadlessDispatcher::HeadlessDispatcher(service::camera::CameraService& camera_service,
                                       service::window::WindowService& window_service,
                                       service::projector::ProjectorService& projector_service,
                                       service::scan::ScanService& scan_service,
                                       service::scan_dataset::ScanDatasetValidator& scan_dataset_validator,
                                       capture::CaptureService& capture_service,
                                       video::CameraManager& cameras,
                                       calib::Calibrator* calibrator, calib::StereoCalibrator* stereo_calibrator,
                                       calib::StereoData& stereo_data)
    : camera_service_(camera_service), window_service_(window_service), projector_service_(projector_service),
      scan_service_(scan_service), scan_dataset_validator_(scan_dataset_validator), capture_service_(capture_service), cameras_(cameras), calibrator_(calibrator),
      stereo_calibrator_(stereo_calibrator), stereo_data_(stereo_data)
{
}

common::CommandResult HeadlessDispatcher::execute(const cmd::Command& command)
{
    if (const auto busy = rejectIfScanResourceBusy(command))
    {
        return *busy;
    }

    runtime::CameraHandlerContext camera_ctx{camera_service_};
    const auto camera_result = handler::camera::handle(camera_ctx, command);
    if (camera_result.handled)
    {
        return camera_result;
    }

    runtime::WindowResourceHandlerContext window_ctx{window_service_};
    const auto window_result = handler::window_resource::handle(window_ctx, command);
    if (window_result.handled)
    {
        return window_result;
    }

    runtime::ProjectorHandlerContext projector_ctx{projector_service_};
    const auto projector_result = handler::projector::handle(projector_ctx, command);
    if (projector_result.handled)
    {
        return projector_result;
    }

    runtime::ScanHandlerContext scan_ctx{scan_service_};
    const auto scan_result = handler::scan::handle(scan_ctx, command);
    if (scan_result.handled)
    {
        return scan_result;
    }

    runtime::ScanDatasetHandlerContext scan_dataset_ctx{scan_dataset_validator_};
    const auto scan_dataset_result = handler::scan_dataset::handle(scan_dataset_ctx, command);
    if (scan_dataset_result.handled)
    {
        return scan_dataset_result;
    }

    runtime::CaptureHandlerContext capture_ctx{capture_service_};
    const auto capture_result = handler::capture::handle(capture_ctx, command);
    if (capture_result.handled)
    {
        return capture_result;
    }

    runtime::MonoCalibrationCalcContext mono_ctx{cameras_, calibrator_};
    const auto mono_result = handler::calibration::handle(mono_ctx, command);
    if (mono_result.handled)
    {
        return mono_result;
    }

    runtime::StereoCalibrationCalcContext stereo_ctx{cameras_, stereo_calibrator_, stereo_data_};
    const auto stereo_result = handler::stereo_calibration::handle(stereo_ctx, command);
    if (stereo_result.handled)
    {
        return stereo_result;
    }

    return common::notHandled();
}

std::optional<common::CommandResult> HeadlessDispatcher::rejectIfScanResourceBusy(const cmd::Command& command) const
{
    return std::visit(
        [this](const auto& typed) -> std::optional<common::CommandResult>
        {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, cmd::CmdOpenProjector>)
            {
                if (const auto busy = busyProjectorResult(scan_service_, typed.projector_role))
                {
                    return busy;
                }
                return busyWindowResult(scan_service_, typed.window_role);
            }
            else if constexpr (std::is_same_v<T, cmd::CmdCloseProjector> ||
                               std::is_same_v<T, cmd::CmdConfigureProjectorSurface> ||
                               std::is_same_v<T, cmd::CmdGeneratePatterns> ||
                               std::is_same_v<T, cmd::CmdProjectorShowPattern> ||
                               std::is_same_v<T, cmd::CmdProjectorNextPattern> ||
                               std::is_same_v<T, cmd::CmdProjectorPrevPattern>)
            {
                return busyProjectorResult(scan_service_, typed.projector_role);
            }
            else if constexpr (std::is_same_v<T, cmd::CmdOpenWindow> || std::is_same_v<T, cmd::CmdCloseWindow>)
            {
                return busyWindowResult(scan_service_, typed.window_role);
            }
            else if constexpr (std::is_same_v<T, cmd::CmdOpenCamera> || std::is_same_v<T, cmd::CmdCloseCamera>)
            {
                return busyCameraResult(scan_service_, typed.role);
            }
            else if constexpr (std::is_same_v<T, cmd::CmdCaptureFrame> || std::is_same_v<T, cmd::CmdCaptureStereo> ||
                               std::is_same_v<T, cmd::CmdCalibCapture>)
            {
                return busyCaptureResult(scan_service_);
            }
            else
            {
                return std::nullopt;
            }
        },
        command);
}

} // namespace headless
