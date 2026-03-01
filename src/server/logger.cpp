#include "logger.hpp"
#include <iostream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace cppsim {
namespace server {

namespace {
    std::mutex log_mutex;
    std::mutex timestamp_mutex;

    std::string get_timestamp() {
      std::lock_guard<std::mutex> lock(timestamp_mutex);
      auto now = std::chrono::system_clock::now();
      auto time_t = std::chrono::system_clock::to_time_t(now);
      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()) % 1000;

      std::tm tm;
#ifdef _WIN32
      localtime_s(&tm, &time_t);
#else
      localtime_r(&time_t, &tm);
#endif

      std::stringstream ss;
      ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
      ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
      return ss.str();
    }

    std::string level_to_string(log_level level) {
      switch (level) {
        case log_level::error:
          return "[ERROR]";
        case log_level::info:
        default:
          return "[INFO]";
      }
    }
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
