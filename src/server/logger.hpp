#pragma once

#include <string>

namespace cppsim {
namespace server {

enum class log_level { info, error };

// Thread-safe logging helpers
void log_message(const std::string& msg);
void log_error(const std::string& msg);
void log(log_level level, const std::string& msg);

} // namespace server
} // namespace cppsim
