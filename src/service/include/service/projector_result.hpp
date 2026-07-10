#pragma once

#include <optional>
#include <string>

namespace service::projector
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

    /// width <int>: projector pattern幅。
    int width{0};

    /// height <int>: projector pattern高さ。
    int height{0};

    /// pattern_count <int>: 生成済みpattern枚数。
    int pattern_count{0};

    /// pattern_index <int>: 表示または選択されたpattern index。
    int pattern_index{-1};

    /// error <std::optional<ProjectorError>>: 失敗時のerror情報。
    std::optional<ProjectorError> error;

    /// @brief 成功したProjectorResultを作成する。
    ///
    /// Args:
    ///   projector_role <std::string>: 対象projector role名。
    ///   window_role <std::string>: 表示先window role名。
    ///   width <int>: projector pattern幅。
    ///   height <int>: projector pattern高さ。
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

} // namespace service::projector
