// target.hpp
#pragma once
#include <variant>
#include "window/window.hpp"

using WindowId = win::Window::Id;

struct TargetAll      {};          // 全ウィンドウ
struct TargetFocused  {};          // アプリ管理の「現在の対象」
struct TargetById     { WindowId id; };

using Target = std::variant<TargetAll, TargetFocused, TargetById>;
