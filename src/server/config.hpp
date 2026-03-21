#pragma once

#include <chrono>

namespace cppsim {
namespace server {

// Server configuration constants
struct config {
    static constexpr auto HANDSHAKE_TIMEOUT = std::chrono::seconds(10);
    static constexpr auto IDLE_TIMEOUT = std::chrono::seconds(60);
    static constexpr size_t MAX_MESSAGE_SIZE = 64 * 1024; // 64KB - sufficient for game state, prevents memory exhaustion
    static constexpr size_t MAX_WRITE_QUEUE_SIZE = 100;   // Maximum pending outgoing messages per session
    static constexpr unsigned short DEFAULT_PORT = 8080;
    static constexpr unsigned short DEFAULT_TEST_PORT = 18080; // Test port to avoid conflicts with production
    static constexpr int MAX_MESSAGES_PER_SECOND = 10;    // Limits DoS attack surface while allowing normal play
    static constexpr int PLACEHOLDER_SEAT = -1;            // Sentinel value for unassigned seat
    static constexpr double PLACEHOLDER_STACK = 0.0;      // Sentinel value for uninitialized stack
    static constexpr size_t MAX_CONNECTIONS = 1000;       // Maximum concurrent connections
    static constexpr size_t MAX_SESSION_ID_LENGTH = 256;  // Maximum length for generated session IDs
    static constexpr size_t MAX_TIMESTAMPS_TO_TRACK = 50; // 5x rate limit for sliding window precision
    static constexpr int MAX_BACKOFF_SECONDS = 30;        // Maximum exponential backoff duration
};

} // namespace server
} // namespace cppsim
