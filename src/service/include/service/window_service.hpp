#pragma once

#include "service/window_result.hpp"

#include <atomic>
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

    /// @brief WindowServiceを破棄し、所有backendを解放する。
    ///
    /// Args:
    ///   なし。
    ///
    /// Return:
    ///   <void>: なし。
    ~WindowService();

    /// @brief windowを作成してroleへbindする。
    ///
    /// Args:
    ///   config <const WindowOpenConfig&>: window作成設定。
    ///
    /// Return:
    ///   <WindowResult>: window作成結果。
    WindowResult openWindow(const WindowOpenConfig& config);

    /// @brief roleに紐づくwindowをcloseする。
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

    /// @brief WindowServiceが所有するwindowをすべてcloseする。
    ///
    /// Args:
    ///   なし。
    ///
    /// Return:
    ///   <void>: なし。
    void closeAll();

    /// @brief window event処理を最小限進める。
    ///
    /// Args:
    ///   delay_ms <int>: event pump待機時間ms。
    ///
    /// Return:
    ///   <void>: なし。
    void pollEvents(int delay_ms = 1);

  private:
    /// @brief window event pump workerを開始する。
    ///
    /// Args:
    ///   なし。
    ///
    /// Return:
    ///   <void>: なし。
    void startEventPump();

    /// @brief window event pump workerを停止する。
    ///
    /// Args:
    ///   なし。
    ///
    /// Return:
    ///   <void>: なし。
    void stopEventPump();

    class WindowManagerBackend;

    /// owned_backend_ <std::unique_ptr<WindowManagerBackend>>: WindowManager adapterの所有領域。
    std::unique_ptr<WindowManagerBackend> owned_backend_;

    /// backend_ <WindowBackend&>: window resourceを管理するbackend。
    WindowBackend& backend_;

    /// role_to_window_id_ <std::unordered_map<std::string, win::WindowId>>: roleからWindowIdへのbinding。
    std::unordered_map<std::string, win::WindowId> role_to_window_id_;

    /// mutex_ <std::mutex>: backend操作とrole bindingを保護するmutex。
    mutable std::mutex mutex_;

    /// pump_running_ <std::atomic_bool>: event pump workerの実行状態。
    std::atomic_bool pump_running_{false};

    /// pump_thread_ <std::thread>: stdin待機中もHighGUI eventを進めるworker。
    std::thread pump_thread_;
};

} // namespace service::window
