#pragma once

#include "window/monitor_service.hpp"

#include <optional>
#include <string>
#include <vector>

namespace projector
{

struct ProjectorError
{
    /// code <std::string>: projector command失敗時のerror code。
    std::string code;

    /// message <std::string>: projector command失敗時のerror message。
    std::string message;
};

struct ProjectorResult
{
    /// ok <bool>: projector commandが成功したか。
    bool ok{false};

    /// projector_role <std::string>: 対象projector role名。
    std::string projector_role;

    /// window_role <std::string>: 表示先window role名。
    std::string window_role;

    /// width <int>: Gray Code論理解像度の幅。
    int width{0};

    /// height <int>: Gray Code論理解像度の高さ。
    int height{0};

    /// code_width <int>: Gray Code論理解像度の幅。
    int code_width{0};

    /// code_height <int>: Gray Code論理解像度の高さ。
    int code_height{0};

    /// pattern_count <int>: 生成済みpattern枚数。
    int pattern_count{0};

    /// pattern_index <int>: 表示または選択されたpattern index。
    int pattern_index{-1};

    /// monitor_index <int>: projector surfaceの対象monitor index。
    int monitor_index{0};

    /// monitor_x <int>: desktop座標上のmonitor左上X座標。
    int monitor_x{0};

    /// monitor_y <int>: desktop座標上のmonitor左上Y座標。
    int monitor_y{0};

    /// monitor_width <int>: projector surface対象monitorの横幅。
    int monitor_width{0};

    /// monitor_height <int>: projector surface対象monitorの縦幅。
    int monitor_height{0};

    /// surface_width <int>: black canvasとして表示するsurface横幅。
    int surface_width{0};

    /// surface_height <int>: black canvasとして表示するsurface縦幅。
    int surface_height{0};

    /// pattern_width <int>: surface内の表示領域横幅。
    int pattern_width{0};

    /// pattern_height <int>: surface内の表示領域縦幅。
    int pattern_height{0};

    /// display_width <int>: surface内の表示領域横幅。
    int display_width{0};

    /// display_height <int>: surface内の表示領域縦幅。
    int display_height{0};

    /// display_x <int>: surface内の表示領域左上X座標。
    int display_x{0};

    /// display_y <int>: surface内の表示領域左上Y座標。
    int display_y{0};

    /// pattern_x <int>: surface内でactive patternを貼る左上X座標。
    int pattern_x{0};

    /// pattern_y <int>: surface内でactive patternを貼る左上Y座標。
    int pattern_y{0};

    /// clamped <bool>: requested値からclampされた場合true。
    bool clamped{false};

    /// monitors <std::vector<win::MonitorInfo>>: list_monitors結果。
    std::vector<win::MonitorInfo> monitors;

    /// error <std::optional<ProjectorError>>: 失敗時のerror情報。
    std::optional<ProjectorError> error;

    /// @brief 成功したProjectorResultを作成する。
    ///
    /// Args:
    ///   projector_role <std::string>: 対象projector role名。
    ///   window_role <std::string>: 表示先window role名。
    ///   width <int>: Gray Code論理解像度の幅。
    ///   height <int>: Gray Code論理解像度の高さ。
    ///   pattern_count <int>: 生成済みpattern枚数。
    ///   pattern_index <int>: 表示または選択されたpattern index。
    ///
    /// Return:
    ///   <ProjectorResult>: ok=trueのprojector command結果。
    static ProjectorResult success(std::string projector_role, std::string window_role, int width, int height,
                                   int pattern_count, int pattern_index);

    /// @brief 失敗したProjectorResultを作成する。
    ///
    /// Args:
    ///   projector_role <std::string>: 対象projector role名。
    ///   code <std::string>: error code。
    ///   message <std::string>: error message。
    ///
    /// Return:
    ///   <ProjectorResult>: ok=falseのprojector command結果。
    static ProjectorResult failure(std::string projector_role, std::string code, std::string message);
};

} // namespace projector
