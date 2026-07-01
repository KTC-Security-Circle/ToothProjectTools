#pragma once

#include <map>
#include <optional>
#include <string>

namespace headless
{

struct HeadlessCommandError
{
    /// code <std::string>: sidecar responseへ返すerror code。
    std::string code;

    /// message <std::string>: sidecar responseへ返すerror message。
    std::string message;
};

struct HeadlessCommandResult
{
    /// handled <bool>: commandがheadless dispatcherで処理対象だったか。
    bool handled{false};

    /// ok <bool>: command実行が成功したか。
    bool ok{false};

    /// error <std::optional<HeadlessCommandError>>: command失敗時のerror情報。
    std::optional<HeadlessCommandError> error;

    /// values <std::map<std::string, std::string>>: responseへ返す追加値。
    std::map<std::string, std::string> values;
};

} // namespace headless
