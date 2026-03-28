#include "logger.hpp"
#include <ctime>
#include <iostream>
#include <limits>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace cppsim {
namespace server {

namespace {
    std::mutex log_mutex;
    std::mutex fallback_mutex;

    std::string get_timestamp() {
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

    [[nodiscard]] const char* level_to_string(log_level level) noexcept {
      switch (level) {
        case log_level::error:
          return "[ERROR]";
        case log_level::info:
        default:
          return "[INFO]";
      }
    }
}

void log(log_level level, std::string_view msg) noexcept {
    try {
      std::string timestamp = get_timestamp();
      std::lock_guard<std::mutex> lock(log_mutex);
      std::ostream& stream = (level == log_level::error) ? std::cerr : std::cout;
      stream << timestamp << " " << level_to_string(level) << " " << msg << '\n';
      stream.flush();
    } catch (...) {
      std::lock_guard<std::mutex> fallback_lock(fallback_mutex);
      constexpr size_t max_safe_size = static_cast<size_t>(std::numeric_limits<int>::max());
      auto safe_size = static_cast<int>(std::min(msg.size(), max_safe_size));
      std::fprintf(stderr, "[FALLBACK] %s %.*s\n",
                   level_to_string(level),
                   safe_size, msg.data());
    }
}

void log_message(std::string_view msg) noexcept {
    log(log_level::info, msg);
}

void log_error(std::string_view msg) noexcept {
    log(log_level::error, msg);
}

}
}
