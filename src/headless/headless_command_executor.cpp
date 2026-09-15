#include "headless/headless_command_executor.hpp"

#include "scan/scan_service.hpp"

#include <string>
#include <type_traits>
#include <variant>

namespace headless
{
namespace
{
std::optional<common::CommandResult> busyProjectorResult(const scan::ScanService& service,
                                                          const std::string& role)
{
    if (!service.isProjectorRoleBusy(role)) return std::nullopt;
    return common::failure("scan_resource_busy", "projector role is used by active scan: " + role);
}

std::optional<common::CommandResult> busyCameraResult(const scan::ScanService& service,
                                                       const std::string& role)
{
    if (!service.isCameraRoleBusy(role)) return std::nullopt;
    return common::failure("scan_resource_busy", "camera role is used by active scan: " + role);
}

std::optional<common::CommandResult> busyWindowResult(const scan::ScanService& service,
                                                       const std::string& role)
{
    if (!service.isWindowRoleBusy(role)) return std::nullopt;
    return common::failure("scan_resource_busy", "window role is used by active scan: " + role);
}
} // namespace

HeadlessCommandExecutor::HeadlessCommandExecutor(
    video::CameraService& camera_service, win::WindowService& window_service,
    projector::ProjectorService& projector_service, scan::ScanService& scan_service,
    scan::dataset::ScanDatasetValidator& scan_dataset_validator,
    decode::DecodeService& decode_service, capture::CaptureService& capture_service,
    video::CameraManager& cameras, calib::Calibrator* calibrator,
    calib::StereoCalibrator* stereo_calibrator, calib::StereoData& stereo_data,
    reconstruction::ReconstructionService& reconstruction_service)
    : camera_service_(camera_service), window_service_(window_service), projector_service_(projector_service),
      scan_service_(scan_service), scan_dataset_validator_(scan_dataset_validator), decode_service_(decode_service),
      capture_service_(capture_service), cameras_(cameras), calibrator_(calibrator),
      stereo_calibrator_(stereo_calibrator), stereo_data_(stereo_data),
      reconstruction_service_(reconstruction_service)
{
}

common::CommandResult HeadlessCommandExecutor::execute(const cmd::Command& command)
{
    return std::visit(
        [this](const auto& typed)
        {
            if (const auto rejection = validateResourceAccess(typed)) return *rejection;
            return executeTyped(typed);
        },
        command);
}

template <typename CommandType>
std::optional<common::CommandResult> HeadlessCommandExecutor::validateResourceAccess(
    const CommandType& command) const
{
    using T = std::decay_t<CommandType>;
    if constexpr (std::is_same_v<T, cmd::CmdOpenProjector>) {
        if (const auto busy = busyProjectorResult(scan_service_, command.projector_role)) return busy;
        return busyWindowResult(scan_service_, command.window_role);
    } else if constexpr (std::is_same_v<T, cmd::CmdCloseProjector> ||
                         std::is_same_v<T, cmd::CmdConfigureProjectorSurface> ||
                         std::is_same_v<T, cmd::CmdGeneratePatterns> ||
                         std::is_same_v<T, cmd::CmdProjectorShowPattern> ||
                         std::is_same_v<T, cmd::CmdProjectorNextPattern> ||
                         std::is_same_v<T, cmd::CmdProjectorPrevPattern>) {
        return busyProjectorResult(scan_service_, command.projector_role);
    } else if constexpr (std::is_same_v<T, cmd::CmdOpenWindow> || std::is_same_v<T, cmd::CmdCloseWindow>) {
        return busyWindowResult(scan_service_, command.window_role);
    } else if constexpr (std::is_same_v<T, cmd::CmdOpenCamera> || std::is_same_v<T, cmd::CmdCloseCamera>) {
        return busyCameraResult(scan_service_, command.role);
    } else if constexpr (std::is_same_v<T, cmd::CmdCaptureFrame> ||
                         std::is_same_v<T, cmd::CmdCaptureStereo> ||
                         std::is_same_v<T, cmd::CmdCalibCapture>) {
        if (scan_service_.isScanActive())
            return common::failure("scan_resource_busy", "capture command conflicts with active scan");
    }
    return std::nullopt;
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdCalibCapture&)
{
    return common::notHandled();
}
} // namespace headless
