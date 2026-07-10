#pragma once

#include "service/window_result.hpp"

#include <condition_variable>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

namespace win
{
class WindowManager;
}

namespace service::window
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

    /// monitor_index <std::optional<int>>: 表示先monitor index。未指定時は既定monitorを使う。
    std::optional<int> monitor_index;

    /// fullscreen <bool>: fullscreen windowとして開くか。
    bool fullscreen{false};
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
    virtual win::WindowId openWindow(const std::string& title, int width, int height,
                                     std::optional<int> monitor_index, bool fullscreen) = 0;

    /// @brief windowをcloseする。
    ///
    /// Args:
    ///   window_id <win::WindowId>: close対象window id。
    ///
    /// Return:
    ///   <bool>: close対象が存在しcloseできた場合はtrue。
    virtual bool closeWindow(win::WindowId window_id) = 0;

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

    /// @brief test用backendを利用するWindowServiceを構築する。
    ///
    /// Args:
    ///   backend <WindowBackend&>: window作成/closeを実行するbackend。
    ///
    /// Return:
    ///   <WindowService>: backend参照を保持するwindow service。
    explicit WindowService(WindowBackend& backend);

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

    /// @brief open中のwindowが存在するか返す。
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
    class WindowManagerBackend;

    /// @brief main thread専用API呼び出し元を検証する。
    ///
    /// Args:
    ///   operation <const char*>: 検証対象操作名。
    ///
    /// Return:
    ///   <bool>: main threadから呼ばれた場合はtrue。
    bool ensureGuiThread(const char* operation) const;

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

} // namespace service::window
