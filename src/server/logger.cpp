#include "logger.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <mutex>
#include <ostream>

namespace cppsim {
namespace server {

namespace {
    std::mutex log_mutex;

    constexpr size_t TIMESTAMP_BUFFER_SIZE = 32;
    constexpr size_t MS_PRECISION = 3;

    std::array<char, TIMESTAMP_BUFFER_SIZE> get_timestamp() {
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

      std::array<char, TIMESTAMP_BUFFER_SIZE> buf;
      size_t len = std::strftime(buf.data(), buf.size(), "%Y-%m-%d %H:%M:%S", &tm);
      if (len == 0) {
        // strftime failed — use a safe fallback (avoid trigraphs)
        std::snprintf(buf.data(), buf.size(), "ts-err.%0*d",
                      static_cast<int>(MS_PRECISION), static_cast<int>(ms.count()));
      } else {
        std::snprintf(buf.data() + len, buf.size() - len, ".%0*d",
                      static_cast<int>(MS_PRECISION), static_cast<int>(ms.count()));
      }
      return buf;
    }
}

void log(log_level level, std::string_view msg) noexcept {
    try {
      auto timestamp = get_timestamp();
      std::lock_guard<std::mutex> lock(log_mutex);
      std::ostream& stream = (level == log_level::error) ? std::cerr : std::cout;
      stream << timestamp.data() << " " << log_level_to_string(level) << " " << msg << '\n';
      stream.flush();
    } catch (...) {
      // Best-effort fallback — fprintf to stderr is atomic on POSIX,
      // no additional synchronization needed.
      constexpr size_t MAX_FALLBACK_MSG_SIZE = 1024;
      std::fprintf(stderr, "[FALLBACK] %s %.*s\n",
                   log_level_to_string(level),
                   static_cast<int>(std::min(msg.size(), MAX_FALLBACK_MSG_SIZE)),
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
