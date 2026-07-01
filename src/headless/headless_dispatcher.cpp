#include "headless/headless_dispatcher.hpp"

#include "capture/capture_result.hpp"
#include "capture/capture_service.hpp"

#include <type_traits>

namespace headless
{
namespace
{

/// @brief CaptureResultをHeadlessCommandResultへ変換する。
///
/// Args:
///   result <const capture::CaptureResult&>: CaptureServiceから返されたcapture_frame結果。
///
/// Return:
///   <HeadlessCommandResult>: sidecar responseへ変換可能なheadless command結果。
HeadlessCommandResult fromCaptureFrameResult(const capture::CaptureResult& result)
{
    HeadlessCommandResult command_result;
    command_result.handled = true;
    command_result.ok = result.ok;

    if (result.ok)
    {
        command_result.values.emplace("path", result.output_path.string());
        return command_result;
    }

    command_result.error = HeadlessCommandError{
        "capture_failed",
        result.error ? result.error->message : std::string{"failed to capture frame"},
    };
    return command_result;
}

/// @brief CaptureStereoResultをHeadlessCommandResultへ変換する。
///
/// Args:
///   result <const capture::CaptureStereoResult&>: CaptureServiceから返されたcapture_stereo結果。
///
/// Return:
///   <HeadlessCommandResult>: sidecar responseへ変換可能なheadless command結果。
HeadlessCommandResult fromCaptureStereoResult(const capture::CaptureStereoResult& result)
{
    HeadlessCommandResult command_result;
    command_result.handled = true;
    command_result.ok = result.ok;

    if (result.ok)
    {
        command_result.values.emplace("left_path", result.left_output_path.string());
        command_result.values.emplace("right_path", result.right_output_path.string());
        return command_result;
    }

    command_result.error = HeadlessCommandError{
        "capture_failed",
        result.error ? result.error->message : std::string{"failed to capture stereo frames"},
    };
    return command_result;
}

} // namespace

HeadlessDispatcher::HeadlessDispatcher(capture::CaptureService& capture_service)
    : capture_service_(capture_service)
{
}

HeadlessCommandResult HeadlessDispatcher::execute(const cmd::Command& command)
{
    return std::visit(
        [this](auto&& c) -> HeadlessCommandResult
        {
            using T = std::decay_t<decltype(c)>;

            if constexpr (std::is_same_v<T, cmd::CmdCaptureFrame>)
            {
                return fromCaptureFrameResult(capture_service_.captureFrame(c.camera_id, c.output_path));
            }
            else if constexpr (std::is_same_v<T, cmd::CmdCaptureStereo>)
            {
                return fromCaptureStereoResult(
                    capture_service_.captureStereo(
                        c.left_camera_id, c.right_camera_id, c.left_output_path, c.right_output_path));
            }
            else
            {
                return HeadlessCommandResult{};
            }
        },
        command);
}

} // namespace headless
