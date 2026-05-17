#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "string_utils.hpp"

namespace cppsim {
namespace server {

// Truncate session ID for safe logging (show prefix + hex chars)
// Length matches: "sess_" (5 chars) + 8 hex chars = 13
constexpr size_t SESSION_ID_LOG_LENGTH = 13;

[[nodiscard]] inline std::string sanitize_session_id(std::string_view sid) {
  if (sid.size() <= SESSION_ID_LOG_LENGTH) return std::string(sid);
  return std::string(sid.substr(0, SESSION_ID_LOG_LENGTH)) + "...";
}

// trunc_field is inherited from common/string_utils.hpp via the include above.

}  // namespace server
}  // namespace cppsim
