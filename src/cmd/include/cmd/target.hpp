// src/cmd/include/cmd/target.hpp
#pragma once
#include <cstdint>
#include <variant>

using WindowId = std::uint64_t;     // ← window から独立

struct TargetAll {};
struct TargetFocused {};
struct TargetById { WindowId id; };

using Target = std::variant<TargetAll, TargetFocused, TargetById>;
