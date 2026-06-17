#include "stream/role_name.hpp"

#include <algorithm>

namespace stream {
namespace {

bool isRoleCharacter(char character) {
  return ('a' <= character && character <= 'z') ||
         ('A' <= character && character <= 'Z') ||
         ('0' <= character && character <= '9') ||
         character == '_' ||
         character == '-';
}

} // namespace

bool isValidRoleName(std::string_view role) {
  return !role.empty() && std::all_of(role.begin(), role.end(), isRoleCharacter);
}

} // namespace stream
