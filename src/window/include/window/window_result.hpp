#pragma once

#include "window/window_types.hpp"

#include <optional>
#include <string>

namespace win
{

struct WindowError
{
    /// code <std::string>: window command失敗時のerror code。
    std::string code;

    /// message <std::string>: window command失敗時のerror message。
    std::string message;
};

struct WindowResult
{
    /// ok <bool>: window commandが成功したか。
    bool ok{false};

    /// role <std::string>: 対象window role名。
    std::string role;

    /// window_id <win::WindowId>: 作成またはcloseしたwindow識別子。
    win::WindowId window_id{win::kInvalidWindowId};

    /// width <int>: 実際に作成されたwindowの横幅。
    int width{0};

    /// height <int>: 実際に作成されたwindowの縦幅。
    int height{0};

    /// error <std::optional<WindowError>>: window command失敗時のerror情報。
    std::optional<WindowError> error;

    /// @brief 成功したWindowResultを作成する。
    ///
    /// Args:
    ///   role <std::string>: 対象window role名。
    ///   window_id <win::WindowId>: 作成またはcloseしたwindow識別子。
    ///   width <int>: window幅。
    ///   height <int>: window高さ。
    ///
    /// Return:
    ///   <WindowResult>: ok=true のwindow command結果。
    static WindowResult success(std::string role, win::WindowId window_id, int width, int height);

    /// @brief 失敗したWindowResultを作成する。
    ///
    /// Args:
    ///   role <std::string>: 対象window role名。
    ///   code <std::string>: error code。
    ///   message <std::string>: error message。
    ///
    /// Return:
    ///   <WindowResult>: ok=false のwindow command結果。
    static WindowResult failure(std::string role, std::string code, std::string message);
};

} // namespace win
