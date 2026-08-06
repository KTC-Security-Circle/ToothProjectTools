#pragma once

#include "service/monitor_service.hpp"
#include "service/projector_result.hpp"

#include <memory>
#include <mutex>
#include <opencv2/core/mat.hpp>
#include <optional>
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

enum class ProjectorPlacement
{
    center,
    custom,
};

struct ProjectorSurface
{
    int monitor_index{0};
    int monitor_x{0};
    int monitor_y{0};
    int monitor_width{0};
    int monitor_height{0};
    int surface_width{0};
    int surface_height{0};
    int pattern_width{0};
    int pattern_height{0};
    int pattern_x{0};
    int pattern_y{0};
    bool clamped{false};
    ProjectorPlacement placement{ProjectorPlacement::center};
};

struct ProjectorSurfaceRequest
{
    std::string projector_role;
    int monitor_index{0};
    int width{0};
    int height{0};
    std::optional<int> x;
    std::optional<int> y;
    ProjectorPlacement placement{ProjectorPlacement::center};
};

struct ProjectorScanSnapshot
{
    /// projector_role <std::string>: projector role名。
    std::string projector_role;

    /// window_role <std::string>: 表示先window role名。
    std::string window_role;

    /// pattern_count <int>: 生成済みpattern数。
    int pattern_count{0};

    /// code_width <int>: Gray Code論理解像度の幅。
    int code_width{0};

    /// code_height <int>: Gray Code論理解像度の高さ。
    int code_height{0};

    /// patterns_dirty <bool>: pattern未生成または生成済みpatternが無効ならtrue。
    bool patterns_dirty{false};

    /// surface <ProjectorSurface>: 現在のprojector surface。
    ProjectorSurface surface;
};

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
    ProjectorService(service::window::WindowService& window_service, service::monitor::MonitorService& monitor_service);

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

    /// @brief 利用可能なmonitor一覧を返す。
    /// TODO: list_monitorsは将来的にMonitorHandlerへ分離する。
    ProjectorResult listMonitors();

    /// @brief projector表示surfaceとactive pattern areaを設定する。
    ProjectorResult configureSurface(const ProjectorSurfaceRequest& request);

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

    /// @brief scan開始前に必要なprojector状態snapshotを取得する。
    std::optional<ProjectorScanSnapshot> scanSnapshot(const std::string& projector_role) const;

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

        /// surface <ProjectorSurface>: monitor/surface/pattern active area設定。
        ProjectorSurface surface;

        /// code_width <int>: Gray Code論理解像度の幅。
        int code_width{0};

        /// code_height <int>: Gray Code論理解像度の高さ。
        int code_height{0};

        /// structured_light <std::unique_ptr<sl::StructuredLight>>: GrayCodePattern管理object。
        std::unique_ptr<sl::StructuredLight> structured_light;

        /// current_index <int>: 現在表示対象のpattern index。
        int current_index{0};

        /// patterns_dirty <bool>: pattern未生成または生成済みpatternが無効ならtrue。
        bool patterns_dirty{true};
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
    const ProjectorSession* findSession(const std::string& projector_role) const;

    /// @brief sessionの現在状態から成功resultを作成する。
    ///
    /// Args:
    ///   session <const ProjectorSession&>: 対象session。
    ///
    /// Return:
    ///   <ProjectorResult>: 成功result。
    static ProjectorResult successFromSession(const ProjectorSession& session);

    ProjectorSurface makeDefaultSurface(int width, int height) const;
    static ProjectorSurface computeSurface(const service::monitor::MonitorInfo& monitor, int requested_width,
                                           int requested_height, std::optional<int> requested_x,
                                           std::optional<int> requested_y, ProjectorPlacement placement);
    ProjectorResult showPatternLocked(const std::string& projector_role, int index);
    static cv::Mat composePatternCanvas(const cv::Mat& pattern, const ProjectorSurface& surface);

    /// window_service_ <service::window::WindowService&>: pattern表示先window service。
    service::window::WindowService& window_service_;

    /// monitor_service_ <service::monitor::MonitorService&>: monitor情報取得service。
    service::monitor::MonitorService& monitor_service_;

    /// mutex_ <std::mutex>: projector session mapとsession状態を保護するmutex。
    mutable std::mutex mutex_;

    /// sessions_ <std::unordered_map<std::string, ProjectorSession>>: projector roleごとのsession。
    std::unordered_map<std::string, ProjectorSession> sessions_;
};

} // namespace service::projector
