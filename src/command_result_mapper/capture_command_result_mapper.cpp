#include "command_result_mapper/capture_command_result_mapper.hpp"

#include <optional>
#include <string>
#include <utility>

namespace command_result_mapper::capture
{
namespace
{

/// @brief CaptureErrorCodeを外部公開用command error codeへ変換する。
///
/// Args:
///   code <::capture::CaptureErrorCode>: Capture domain内のerror code。
///
/// Return:
///   <std::string>: CommandResult.error.codeへ設定するsnake_case error code。
std::string toCommandErrorCode(::capture::CaptureErrorCode code)
{
    switch (code)
    {
    case ::capture::CaptureErrorCode::CameraNotFound:
        return "camera_not_found";
    case ::capture::CaptureErrorCode::CameraNotOpen:
        return "camera_not_open";
    case ::capture::CaptureErrorCode::EmptyFrame:
        return "empty_frame";
    case ::capture::CaptureErrorCode::InvalidOutputPath:
        return "invalid_output_path";
    case ::capture::CaptureErrorCode::DirectoryCreateFailed:
        return "directory_create_failed";
    case ::capture::CaptureErrorCode::FileWriteFailed:
        return "file_write_failed";
    case ::capture::CaptureErrorCode::InternalError:
        return "internal_error";
    }

    return "internal_error";
}

/// @brief CaptureErrorからCommandResult失敗値を作成する。
///
/// Args:
///   error <const std::optional<::capture::CaptureError>&>: CaptureServiceから返されたerror情報。
///
/// Return:
///   <common::CommandResult>: capture失敗を表すcommand実行結果。
common::CommandResult captureFailure(const std::optional<::capture::CaptureError>& error)
{
    if (error)
    {
        return common::failure(toCommandErrorCode(error->code), error->message);
    }

    return common::failure("capture_failed", "capture failed without error detail");
}

} // namespace

common::CommandResult toCommandResult(
    const ::capture::CaptureResult& result,
    std::map<std::string, std::string> values)
{
    if (result.ok)
    {
        return common::success(std::move(values));
    }

    return captureFailure(result.error);
}

common::CommandResult toCommandResult(
    const ::capture::CaptureStereoResult& result,
    std::map<std::string, std::string> values)
{
    if (result.ok)
    {
        return common::success(std::move(values));
    }

    return captureFailure(result.error);
}

} // namespace command_result_mapper::capture
