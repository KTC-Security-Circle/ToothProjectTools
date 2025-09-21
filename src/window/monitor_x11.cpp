#include "window/monitor.hpp"
#include "logger/logger_macros.hpp"  // SPDLOG_INFO, SPDLOG_WARN, LOG_ERROR などを定義

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include <vector>

namespace win {

/**
 * @brief X11 + XRandR を利用して、接続されているモニタの矩形情報を列挙する。
 *
 * ディスプレイ環境から有効な CRTC (出力) を取得し、
 * モニタの位置 (x, y) とサイズ (width, height) を MonitorRect として返す。
 *
 * @return std::vector<MonitorRect>
 *   - モニタ矩形リスト（モニタ1台につき1要素）
 *   - 取得に失敗した場合は空ベクトル
 *
 * @note
 *   - XOpenDisplay に失敗した場合は空ベクトルを返す。
 *   - XRandR 拡張が無効な場合も空ベクトルとなる。
 */
std::vector<MonitorRect> enumerate_monitors_x11() {
  std::vector<MonitorRect> monitors;

  Display* display = XOpenDisplay(nullptr);
  if (!display) {
    LOG_ERROR("XOpenDisplay に失敗しました: X ディスプレイを開けません");
    return monitors;
  }

  const Window root = DefaultRootWindow(display);
  XRRScreenResources* screen_resources = XRRGetScreenResources(display, root);
  if (!screen_resources) {
    LOG_ERROR("XRRGetScreenResources に失敗しました");
    XCloseDisplay(display);
    return monitors;
  }

  for (int i = 0; i < screen_resources->ncrtc; ++i) {
    XRRCrtcInfo* crtc_info =
        XRRGetCrtcInfo(display, screen_resources, screen_resources->crtcs[i]);
    if (crtc_info && crtc_info->noutput > 0 && crtc_info->mode != None) {
      monitors.push_back(MonitorRect{
          static_cast<int>(crtc_info->x),
          static_cast<int>(crtc_info->y),
          static_cast<int>(crtc_info->width),
          static_cast<int>(crtc_info->height)});

      SPDLOG_INFO("モニタ {}: 位置=({}, {}), サイズ={}x{}",
                  i, crtc_info->x, crtc_info->y,
                  crtc_info->width, crtc_info->height);
    }
    if (crtc_info) XRRFreeCrtcInfo(crtc_info);
  }

  XRRFreeScreenResources(screen_resources);
  XCloseDisplay(display);

  SPDLOG_INFO("検出されたモニタ数: {}", monitors.size());
  return monitors;
}

} // namespace win
