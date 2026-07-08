#pragma once

#include <map>
#include <optional>
#include <string>
#include <utility>

namespace common
{

/// @brief command実行失敗時の共通error情報。
struct CommandError
{
    /// code <std::string>: command実行失敗時のerror code。
    std::string code;

    /// message <std::string>: command実行失敗時のerror message。
    std::string message;
};

/// @brief handler/dispatcherで共通利用するcommand実行結果。
struct CommandResult
{
    /// handled <bool>: commandがhandlerまたはdispatcherで処理対象だったか。
    bool handled{false};

    /// ok <bool>: command実行が成功したか。
    bool ok{false};

    /// error <std::optional<CommandError>>: command失敗時のerror情報。
    std::optional<CommandError> error;

    /// values <std::map<std::string, std::string>>: command実行結果として返す追加値。
    std::map<std::string, std::string> values;
};

/// @brief command未処理resultを作成する。
///
/// Args:
///   なし。
///
/// Return:
///   <CommandResult>: handled=false, ok=false のcommand result。
inline CommandResult notHandled()
{
    return {};
}

/// @brief command成功resultを作成する。
///
/// Args:
///   values <std::map<std::string, std::string>>: command成功時に返す追加値。
///
/// Return:
///   <CommandResult>: handled=true, ok=true のcommand result。
inline CommandResult success(std::map<std::string, std::string> values = {})
{
    CommandResult result;
    result.handled = true;
    result.ok = true;
    result.values = std::move(values);
    return result;
}

/// @brief command失敗resultを作成する。
///
/// Args:
///   code <std::string>: command失敗時のerror code。
///   message <std::string>: command失敗時のerror message。
///
/// Return:
///   <CommandResult>: handled=true, ok=false のcommand result。
inline CommandResult failure(std::string code, std::string message)
{
    CommandResult result;
    result.handled = true;
    result.error = CommandError{std::move(code), std::move(message)};
    return result;
}

} // namespace common
