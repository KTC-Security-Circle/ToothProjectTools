#pragma once

#include "video/video_types.hpp"
#include "window/window_types.hpp"

#include <variant>

namespace cmd
{

struct TargetAll {};
struct TargetFocus {};

struct TargetWindow
{
    win::WindowId window_id;
};

struct TargetCamera
{
    video::CameraId camera_id;
};

using Target = std::variant<TargetAll, TargetFocus, TargetWindow, TargetCamera>;

} // namespace cmd
