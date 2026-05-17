#pragma once

#include <string>
#include <string_view>

namespace cppsim {
namespace server {

// Truncate session ID for safe logging (show prefix + hex chars)
// Length matches: "sess_" (5 chars) + 8 hex chars = 13
constexpr size_t SESSION_ID_LOG_LENGTH = 13;

[[nodiscard]] inline std::string sanitize_session_id(std::string_view sid) {
  if (sid.size() <= SESSION_ID_LOG_LENGTH) return std::string(sid);
  return std::string(sid.substr(0, SESSION_ID_LOG_LENGTH)) + "...";
}

// Truncate any string field to max_len characters, appending "..." if truncated.
// Useful for safe logging of untrusted input.
[[nodiscard]] inline std::string trunc_field(std::string_view s, size_t max_len = 64) {
  if (s.size() <= max_len) return std::string(s);
  return std::string(s.substr(0, max_len)) + "...";
}

}  // namespace server
}  // namespace cppsim
