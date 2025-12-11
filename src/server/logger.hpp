#pragma once

#include <string>

namespace cppsim {
namespace server {

// Thread-safe logging helpers
void log_message(const std::string& msg);
void log_error(const std::string& msg);

} // namespace server
} // namespace cppsim
