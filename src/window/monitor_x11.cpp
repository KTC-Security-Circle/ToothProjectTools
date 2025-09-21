#include "window/monitor.hpp"

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include <vector>

namespace win {

std::vector<MonitorRect> enumerate_monitors_x11() {
  std::vector<MonitorRect> monitors;

  Display* display = XOpenDisplay(nullptr);
  if (!display) return monitors;

  const Window root = DefaultRootWindow(display);
  XRRScreenResources* screen_resources = XRRGetScreenResources(display, root);
  if (!screen_resources) { XCloseDisplay(display); return monitors; }

  for (int i = 0; i < screen_resources->ncrtc; ++i) {
    XRRCrtcInfo* crtc_info = XRRGetCrtcInfo(display, screen_resources, screen_resources->crtcs[i]);
    if (crtc_info && crtc_info->noutput > 0 && crtc_info->mode != None) {
      monitors.push_back(MonitorRect{
        static_cast<int>(crtc_info->x),
        static_cast<int>(crtc_info->y),
        static_cast<int>(crtc_info->width),
        static_cast<int>(crtc_info->height)
      });
    }
    if (crtc_info) XRRFreeCrtcInfo(crtc_info);
  }

  XRRFreeScreenResources(screen_resources);
  XCloseDisplay(display);
  return monitors;
}

} // namespace win
