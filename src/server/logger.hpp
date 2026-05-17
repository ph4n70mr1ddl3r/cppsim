#pragma once

#include <string_view>

namespace cppsim {
namespace server {

enum class log_level { info, error };

[[nodiscard]] constexpr const char* log_level_to_string(log_level level) noexcept {
  return level == log_level::error ? "[ERROR]" : "[INFO]";
}

void log_message(std::string_view msg) noexcept;
void log_error(std::string_view msg) noexcept;
void log(log_level level, std::string_view msg) noexcept;

}
}
