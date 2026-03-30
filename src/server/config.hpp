#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace cppsim {
namespace server {

// Server configuration constants
// Using inline constexpr for proper ODR compliance with non-integral types in C++17
struct config {
    config() = delete;

    static constexpr auto HANDSHAKE_TIMEOUT = std::chrono::seconds(10);
    static constexpr auto IDLE_TIMEOUT = std::chrono::seconds(60);
    
    static constexpr size_t MAX_MESSAGE_SIZE = 64 * 1024;
    static constexpr size_t MAX_WRITE_QUEUE_SIZE = 100;
    
    static constexpr unsigned short DEFAULT_PORT = 8080;
    static constexpr unsigned short DEFAULT_TEST_PORT = 18080;
    
    static constexpr size_t MAX_MESSAGES_PER_WINDOW = 10;
    static constexpr std::chrono::milliseconds RATE_LIMIT_WINDOW{1000};
    static constexpr size_t MAX_TIMESTAMPS_TO_TRACK = 50;
    
    static constexpr size_t MAX_CONNECTIONS = 1000;
    static constexpr size_t MAX_SESSION_ID_LENGTH = 256;
    static constexpr int MAX_BACKOFF_SECONDS = 30;
    
    static constexpr int PLACEHOLDER_SEAT = -1;
    static constexpr double PLACEHOLDER_STACK = 0.0;
    
    static constexpr int64_t MAX_SEQUENCE_GAP = 10000;
    
    static_assert(MAX_TIMESTAMPS_TO_TRACK >= MAX_MESSAGES_PER_WINDOW,
                  "MAX_TIMESTAMPS_TO_TRACK must be >= MAX_MESSAGES_PER_WINDOW for rate limiting to work correctly");
};

} // namespace server
} // namespace cppsim
