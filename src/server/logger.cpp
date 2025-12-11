#include "logger.hpp"
#include <iostream>
#include <mutex>

namespace cppsim {
namespace server {

namespace {
    std::mutex log_mutex;
}

void log_message(const std::string& msg) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::cout << msg << std::endl;
}

void log_error(const std::string& msg) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::cerr << msg << std::endl;
}

} // namespace server
} // namespace cppsim
