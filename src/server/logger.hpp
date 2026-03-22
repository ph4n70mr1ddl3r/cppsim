#pragma once

#include <string_view>

namespace cppsim {
namespace server {

enum class log_level { info, error };

void log_message(std::string_view msg) noexcept;
void log_error(std::string_view msg) noexcept;
void log(log_level level, std::string_view msg) noexcept;

}
}
