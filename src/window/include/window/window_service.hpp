#pragma once

#include "window/monitor_service.hpp"
#include "window/window_placement_service.hpp"
#include "window/window_result.hpp"

#include <condition_variable>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <opencv2/core/mat.hpp>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

namespace win
{
class WindowManager;
}

namespace win
{

struct WindowOpenConfig
{
    /// role <std::string>: runtime内でwindowを参照するrole名。
    std::string role;

    /// title <std::string>: window title。
    std::string title;

    /// width <int>: 作成するwindowの横幅。
    int width{0};

    /// height <int>: 作成するwindowの縦幅。
    int height{0};

    /// monitor_index <std::optional<int>>: 表示先monitor index。public APIでは0-based。未指定時は既定monitorを使う。
    std::optional<int> monitor_index;

    /// fullscreen <bool>: fullscreen windowとして開くか。
    bool fullscreen{false};
};

struct WindowSurfaceConfig
{
    /// window_role <std::string>: 対象window role名。
    std::string window_role;

    /// monitor_index <int>: 表示先monitor index。public APIでは0-based。
    int monitor_index{0};

    /// x <int>: desktop座標上のwindow左上X座標。
    int x{0};

    /// y <int>: desktop座標上のwindow左上Y座標。
    int y{0};

    /// width <int>: window surface横幅。
    int width{0};

    /// height <int>: window surface縦幅。
    int height{0};

    /// fullscreen <bool>: fullscreen指定。
    bool fullscreen{true};
};

class WindowBackend
{
  public:
    /// @brief backendを破棄する。
    ///
    /// Args:
    ///   なし。
    ///
    /// Return:
    ///   <void>: なし。
    virtual ~WindowBackend() = default;

    /// @brief windowを作成する。
    ///
    /// Args:
    ///   title <const std::string&>: window title。
    ///   width <int>: window幅。
    ///   height <int>: window高さ。
    ///   monitor_index <std::optional<int>>: 表示先monitor index。
    ///   fullscreen <bool>: fullscreenで開くか。
    ///
    /// Return:
    ///   <win::WindowId>: 作成されたwindow id。
    virtual win::WindowId openWindow(const std::string& title, int width, int height) = 0;

    /// @brief windowをcloseする。
    ///
    /// Args:
    ///   window_id <win::WindowId>: close対象window id。
    ///
    /// Return:
    ///   <bool>: close対象が存在しcloseできた場合はtrue。
    virtual bool closeWindow(win::WindowId window_id) = 0;

    /// @brief windowへ画像を表示する。
    ///
    /// Args:
    ///   window_id <win::WindowId>: 表示対象window id。
    ///   image <const cv::Mat&>: 表示する画像。
    ///
    /// Return:
    ///   <bool>: 表示対象が存在し表示できた場合はtrue。
    virtual bool showImage(win::WindowId window_id, const cv::Mat& image) = 0;

    /// @brief window surfaceを再設定する。
    virtual bool configureWindowSurface(win::WindowId window_id, int monitor_index, int x, int y, int width, int height,
                                        bool fullscreen) = 0;

    /// @brief window event処理を進める。
    ///
    /// Args:
    ///   delay_ms <int>: event pump待機時間ms。
    ///
    /// Return:
    ///   <void>: なし。
    virtual void pollEvents(int delay_ms) = 0;
};

class WindowService
{
  public:
    /// @brief WindowManagerを利用するWindowServiceを構築する。
    ///
    /// Args:
    ///   windows <win::WindowManager&>: window resourceを管理するmanager。
    ///
    /// Return:
    ///   <WindowService>: WindowManagerを参照するwindow service。
    explicit WindowService(win::WindowManager& windows);
    WindowService(win::WindowManager& windows, win::MonitorService& monitor_service);
    WindowService(win::WindowManager& windows, win::MonitorService& monitor_service,
                  win::WindowPlacementService& placement_service);

    /// @brief test用backendを利用するWindowServiceを構築する。
    ///
    /// Args:
    ///   backend <WindowBackend&>: window作成/closeを実行するbackend。
    ///
    /// Return:
    ///   <WindowService>: backend参照を保持するwindow service。
    explicit WindowService(WindowBackend& backend);
    WindowService(WindowBackend& backend, win::MonitorService& monitor_service);
    WindowService(WindowBackend& backend, win::MonitorService& monitor_service,
                  win::WindowPlacementService& placement_service);

    /// @brief WindowServiceを破棄する。
    ///
    /// Args:
    ///   なし。
    ///
    /// Return:
    ///   <void>: なし。
    ~WindowService();

    /// @brief window作成requestをmain threadへenqueueして結果を待つ。
    ///
    /// Args:
    ///   config <const WindowOpenConfig&>: window作成設定。
    ///
    /// Return:
    ///   <WindowResult>: window作成結果。
    WindowResult openWindow(const WindowOpenConfig& config);

    /// @brief window close requestをmain threadへenqueueして結果を待つ。
    ///
    /// Args:
    ///   role <const std::string&>: close対象window role名。
    ///
    /// Return:
    ///   <WindowResult>: window close結果。
    WindowResult closeWindow(const std::string& role);

    /// @brief windowへ画像を表示する。
    ///
    /// Args:
    ///   role <const std::string&>: 表示対象window role名。
    ///   image <const cv::Mat&>: 表示する画像。
    ///
    /// Return:
    ///   <WindowResult>: 表示結果。
    WindowResult showImage(const std::string& role, const cv::Mat& image);

    /// @brief open済みwindowのsurfaceを再設定する。
    WindowResult configureWindowSurface(const WindowSurfaceConfig& config);

    /// @brief window roleがopen済みかmain threadへ問い合わせる。
    ///
    /// Args:
    ///   role <const std::string&>: 確認対象window role名。
    ///
    /// Return:
    ///   <bool>: open済みならtrue。
    bool isWindowOpen(const std::string& role);

    /// @brief roleに紐づくWindowIdを取得する。
    ///
    /// Args:
    ///   role <const std::string&>: 解決対象window role名。
    ///
    /// Return:
    ///   <std::optional<win::WindowId>>: bind済みならWindowId。
    std::optional<win::WindowId> resolveWindowId(const std::string& role) const;

    /// @brief WindowServiceが所有するwindowをすべてmain threadでcloseするrequestをenqueueして結果を待つ。
    ///
    /// Args:
    ///   なし。
    ///
    /// Return:
    ///   <void>: なし。
    void closeAll();

    /// @brief queueされたwindow requestをmain thread上で処理する。
    ///
    /// Args:
    ///   なし。
    ///
    /// Return:
    ///   <void>: なし。
    void processPendingRequests();

    /// @brief HighGUI eventをmain thread上で進める。
    ///
    /// Args:
    ///   delay_ms <int>: event pump待機時間ms。
    ///
    /// Return:
    ///   <void>: なし。
    void pollEvents(int delay_ms = 1);

    /// @brief open中windowがあるか返す。GUI thread専用。
    ///
    /// Args:
    ///   なし。
    ///
    /// Return:
    ///   <bool>: open中windowがあればtrue。
    bool hasOpenWindows() const;

    /// @brief main thread上で全windowを即時closeする。
    ///
    /// Args:
    ///   なし。
    ///
    /// Return:
    ///   <void>: なし。
    void closeAllOnMainThread();

  private:
    struct WindowRequest;
    struct OpenWindowRequest;
    struct CloseWindowRequest;
    struct CloseAllWindowsRequest;
    struct ShowImageRequest;
    struct ConfigureWindowSurfaceRequest;
    struct CheckWindowOpenRequest;
    class WindowManagerBackend;
    class BackendPlacementAdapter;

    /// @brief main thread専用API呼び出し元を検証する。
    ///
    /// Args:
    ///   operation <const char*>: 検証対象操作名。
    ///
    /// Return:
    ///   <bool>: main threadから呼ばれた場合はtrue。
    bool ensureGuiThread(const char* operation) const;

    /// @brief 呼び出し元がGUI threadか返す。
    bool isGuiThread() const;

    /// @brief window requestをqueueへ追加する。
    ///
    /// Args:
    ///   request <std::shared_ptr<WindowRequest>>: main threadで処理するrequest。
    ///
    /// Return:
    ///   <void>: なし。
    void enqueue(std::shared_ptr<WindowRequest> request);

    /// @brief open requestをmain thread上で実行する。
    ///
    /// Args:
    ///   request <OpenWindowRequest&>: 処理対象request。
    ///
    /// Return:
    ///   <WindowResult>: window open結果。
    WindowResult executeOpenWindow(OpenWindowRequest& request);

    /// @brief close requestをmain thread上で実行する。
    ///
    /// Args:
    ///   request <CloseWindowRequest&>: 処理対象request。
    ///
    /// Return:
    ///   <WindowResult>: window close結果。
    WindowResult executeCloseWindow(CloseWindowRequest& request);

    /// @brief show image requestをmain thread上で実行する。
    ///
    /// Args:
    ///   request <ShowImageRequest&>: 処理対象request。
    ///
    /// Return:
    ///   <WindowResult>: window表示結果。
    WindowResult executeShowImage(ShowImageRequest& request);

    /// @brief configure surface requestをmain thread上で実行する。
    WindowResult executeConfigureWindowSurface(ConfigureWindowSurfaceRequest& request);

    /// @brief window open確認requestをmain thread上で実行する。
    ///
    /// Args:
    ///   request <CheckWindowOpenRequest&>: 処理対象request。
    ///
    /// Return:
    ///   <bool>: open済みならtrue。
    bool executeCheckWindowOpen(CheckWindowOpenRequest& request);

    /// @brief close all requestをmain thread上で実行する。
    ///
    /// Args:
    ///   request <CloseAllWindowsRequest&>: 処理対象request。
    ///
    /// Return:
    ///   <WindowResult>: window close all結果。
    WindowResult executeCloseAllWindows(CloseAllWindowsRequest& request);

    /// owned_backend_ <std::unique_ptr<WindowManagerBackend>>: WindowManager adapterの所有領域。
    std::unique_ptr<WindowManagerBackend> owned_backend_;

    /// backend_ <WindowBackend&>: window resourceを管理するbackend。
    WindowBackend& backend_;

    std::unique_ptr<win::MonitorService> owned_monitor_service_;
    win::MonitorService& monitor_service_;
    std::unique_ptr<BackendPlacementAdapter> owned_placement_backend_;
    std::unique_ptr<win::WindowPlacementService> owned_placement_service_;
    win::WindowPlacementService& placement_service_;

    /// gui_thread_id_ <std::thread::id>: HighGUI操作を実行するprocess main thread id。
    std::thread::id gui_thread_id_;

    /// role_to_window_id_ <std::unordered_map<std::string, win::WindowId>>: roleからWindowIdへのbinding。
    std::unordered_map<std::string, win::WindowId> role_to_window_id_;

    /// role_to_window_title_ <std::unordered_map<std::string, std::string>>: roleからHighGUI window名へのbinding。
    std::unordered_map<std::string, std::string> role_to_window_title_;

    /// title_to_window_role_ <std::unordered_map<std::string, std::string>>: HighGUI window名からroleへのbinding。
    std::unordered_map<std::string, std::string> title_to_window_role_;

    /// queue_mutex_ <std::mutex>: request queueのpush/popだけを保護するmutex。
    mutable std::mutex queue_mutex_;

    /// requests_ <std::deque<std::shared_ptr<WindowRequest>>>: main threadで処理するwindow request queue。
    std::deque<std::shared_ptr<WindowRequest>> requests_;
};

} // namespace win
