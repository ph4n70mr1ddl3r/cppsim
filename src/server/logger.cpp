#include "logger.hpp"
#include <array>
#include <cstdio>
#include <ctime>
#include <chrono>
#include <iostream>
#include <limits>
#include <mutex>

namespace cppsim {
namespace server {

namespace {
    std::mutex log_mutex;
    std::mutex fallback_mutex;

    std::array<char, 32> get_timestamp() {
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

      std::array<char, 32> buf;
      size_t len = std::strftime(buf.data(), buf.size(), "%Y-%m-%d %H:%M:%S", &tm);
      std::snprintf(buf.data() + len, buf.size() - len, ".%03d",
                    static_cast<int>(ms.count()));
      return buf;
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
      auto timestamp = get_timestamp();
      std::lock_guard<std::mutex> lock(log_mutex);
      std::ostream& stream = (level == log_level::error) ? std::cerr : std::cout;
      stream << timestamp.data() << " " << level_to_string(level) << " " << msg << '\n';
      stream.flush();
    } catch (...) {
      std::lock_guard<std::mutex> fallback_lock(fallback_mutex);
      std::fprintf(stderr, "[FALLBACK] %s %.*s\n",
                   level_to_string(level),
                   static_cast<int>(msg.size() > 1024 ? 1024 : msg.size()),
                   msg.data());
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
