#pragma once

#include <string>
#include <string_view>

namespace cppsim {
namespace server {

// Truncate session ID for safe logging (show first 13 chars: "sess_" + 8 hex chars)
[[nodiscard]] inline std::string sanitize_session_id(std::string_view sid) {
  if (sid.size() <= 13) return std::string(sid);
  return std::string(sid.substr(0, 13)) + "...";
}

}  // namespace server
}  // namespace cppsim
