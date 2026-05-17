#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace cppsim {

// Truncate any string field to max_len characters, appending "..." if truncated.
// Useful for safe logging of untrusted input.
[[nodiscard]] inline std::string trunc_field(std::string_view s, size_t max_len = 64) {
  if (s.size() <= max_len) return std::string(s);
  return std::string(s.substr(0, max_len)) + "...";
}

}  // namespace cppsim
