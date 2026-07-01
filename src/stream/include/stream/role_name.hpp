#pragma once

#include <string_view>

namespace stream {

// MJPEG path segment / sidecar role名として許可する文字だけを判定する。
// std::isalnum はlocale依存になり得るため、URL path用にはASCII固定で判定する。
bool isValidRoleName(std::string_view role);

} // namespace stream
