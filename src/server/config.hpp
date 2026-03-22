#pragma once

#include <chrono>
#include <cstddef>

namespace cppsim {
namespace server {

// Server configuration constants
// Using inline constexpr for proper ODR compliance with non-integral types in C++17
struct config {
    static inline constexpr auto HANDSHAKE_TIMEOUT = std::chrono::seconds(10);
    static inline constexpr auto IDLE_TIMEOUT = std::chrono::seconds(60);
    static inline constexpr size_t MAX_MESSAGE_SIZE = 64 * 1024;
    static inline constexpr size_t MAX_WRITE_QUEUE_SIZE = 100;
    static inline constexpr unsigned short DEFAULT_PORT = 8080;
    static inline constexpr unsigned short DEFAULT_TEST_PORT = 18080;
    static inline constexpr size_t MAX_MESSAGES_PER_SECOND = 10;
    static inline constexpr int PLACEHOLDER_SEAT = -1;
    static inline constexpr double PLACEHOLDER_STACK = 0.0;
    static inline constexpr size_t MAX_CONNECTIONS = 1000;
    static inline constexpr size_t MAX_SESSION_ID_LENGTH = 256;
    static inline constexpr size_t MAX_TIMESTAMPS_TO_TRACK = 50;
    static inline constexpr int MAX_BACKOFF_SECONDS = 30;
    static inline constexpr std::chrono::milliseconds RATE_LIMIT_WINDOW{1000};
};

} // namespace server
} // namespace cppsim
