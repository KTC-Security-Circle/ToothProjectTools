#include "handler/capture_command_result_mapper.hpp"

#include <utility>

namespace handler::capture
{
namespace
{

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
        return common::failure(::capture::toString(error->code), error->message);
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

} // namespace handler::capture
