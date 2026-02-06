#include "logger.hpp"
#include <iostream>
#include <mutex>

namespace cppsim {
namespace server {

namespace {
    std::mutex log_mutex;
}

void log(log_level level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::ostream& stream = (level == log_level::error) ? std::cerr : std::cout;
    stream << get_timestamp() << " " << level_to_string(level) << " " << msg << std::endl;
}

void log_message(const std::string& msg) {
    log(log_level::info, msg);
}

void log_error(const std::string& msg) {
    log(log_level::error, msg);
}

} // namespace server
} // namespace cppsim
