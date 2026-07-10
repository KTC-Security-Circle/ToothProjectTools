#pragma once

#include "service/projector_result.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace sl
{
class StructuredLight;
}

namespace service::window
{
class WindowService;
}

namespace service::projector
{

struct ProjectorOpenConfig
{
    /// projector_role <std::string>: runtime内でprojectorを参照するrole名。
    std::string projector_role;

    /// window_role <std::string>: pattern表示先のwindow role名。
    std::string window_role;

    /// width <int>: GrayCodePattern生成に使う横幅。
    int width{0};

    /// height <int>: GrayCodePattern生成に使う縦幅。
    int height{0};
};

class ProjectorService
{
  public:
    /// @brief WindowServiceを出力先としてProjectorServiceを構築する。
    ///
    /// Args:
    ///   window_service <service::window::WindowService&>: pattern表示先window service。
    ///
    /// Return:
    ///   <ProjectorService>: WindowService backendを持つprojector service。
    explicit ProjectorService(service::window::WindowService& window_service);

    /// @brief ProjectorServiceを破棄する。
    ///
    /// Args:
    ///   なし。
    ///
    /// Return:
    ///   <void>: なし。
    ~ProjectorService();

    /// @brief projector roleをwindow roleへbindする。
    ///
    /// Args:
    ///   config <const ProjectorOpenConfig&>: projector open設定。
    ///
    /// Return:
    ///   <ProjectorResult>: projector open結果。
    ProjectorResult openProjector(const ProjectorOpenConfig& config);

    /// @brief projector roleのbindingを解除する。
    ///
    /// Args:
    ///   projector_role <const std::string&>: close対象projector role名。
    ///
    /// Return:
    ///   <ProjectorResult>: projector close結果。
    ProjectorResult closeProjector(const std::string& projector_role);

    /// @brief projector解像度に合わせてGrayCodePatternを生成する。
    ///
    /// Args:
    ///   projector_role <const std::string&>: pattern生成対象projector role名。
    ///
    /// Return:
    ///   <ProjectorResult>: pattern生成結果。
    ProjectorResult generatePatterns(const std::string& projector_role);

    /// @brief 指定indexのpatternをprojectorへ表示する。
    ///
    /// Args:
    ///   projector_role <const std::string&>: pattern表示対象projector role名。
    ///   index <int>: 表示するpattern index。
    ///
    /// Return:
    ///   <ProjectorResult>: pattern表示結果。
    ProjectorResult showPattern(const std::string& projector_role, int index);

    /// @brief 次のpatternへ進めて表示する。
    ///
    /// Args:
    ///   projector_role <const std::string&>: 操作対象projector role名。
    ///
    /// Return:
    ///   <ProjectorResult>: pattern表示結果。
    ProjectorResult nextPattern(const std::string& projector_role);

    /// @brief 前のpatternへ戻して表示する。
    ///
    /// Args:
    ///   projector_role <const std::string&>: 操作対象projector role名。
    ///
    /// Return:
    ///   <ProjectorResult>: pattern表示結果。
    ProjectorResult prevPattern(const std::string& projector_role);

    /// @brief 全projector bindingを解除する。
    ///
    /// Args:
    ///   なし。
    ///
    /// Return:
    ///   <void>: なし。
    void closeAll();

  private:
    struct ProjectorSession
    {
        /// projector_role <std::string>: runtime内projector role名。
        std::string projector_role;

        /// window_role <std::string>: 表示先window role名。
        std::string window_role;

        /// width <int>: pattern幅。
        int width{0};

        /// height <int>: pattern高さ。
        int height{0};

        /// structured_light <std::unique_ptr<sl::StructuredLight>>: GrayCodePattern管理object。
        std::unique_ptr<sl::StructuredLight> structured_light;

        /// current_index <int>: 現在表示対象のpattern index。
        int current_index{0};
    };

    /// @brief projector role文字列を検証する。
    ///
    /// Args:
    ///   projector_role <const std::string&>: 検証対象projector role名。
    ///
    /// Return:
    ///   <bool>: 有効ならtrue。
    static bool isValidProjectorRole(const std::string& projector_role);

    /// @brief projector sessionを取得する。
    ///
    /// Args:
    ///   projector_role <const std::string&>: 検索対象projector role名。
    ///
    /// Return:
    ///   <ProjectorSession*>: open済みならsession。未openならnullptr。
    ProjectorSession* findSession(const std::string& projector_role);

    /// @brief sessionの現在状態から成功resultを作成する。
    ///
    /// Args:
    ///   session <const ProjectorSession&>: 対象session。
    ///
    /// Return:
    ///   <ProjectorResult>: 成功result。
    static ProjectorResult successFromSession(const ProjectorSession& session);

    /// window_service_ <service::window::WindowService&>: pattern表示先window service。
    service::window::WindowService& window_service_;

    /// sessions_ <std::unordered_map<std::string, ProjectorSession>>: projector roleごとのsession。
    std::unordered_map<std::string, ProjectorSession> sessions_;
};

} // namespace service::projector
