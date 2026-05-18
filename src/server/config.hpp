#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

#include "protocol.hpp"

namespace cppsim {
namespace server {

// Server configuration constants
// Using inline constexpr for proper ODR compliance with non-integral types in C++17
struct config {
    config() = delete;

    static constexpr auto HANDSHAKE_TIMEOUT = std::chrono::seconds{10};
    static constexpr auto IDLE_TIMEOUT = std::chrono::seconds{60};
    
    static constexpr size_t MAX_MESSAGE_SIZE = 64 * 1024;
    static constexpr size_t MAX_WRITE_QUEUE_SIZE = 100;
    
    static constexpr unsigned short DEFAULT_PORT = 8080;
    static constexpr unsigned short DEFAULT_TEST_PORT = 18080;
    
    static constexpr size_t MAX_MESSAGES_PER_WINDOW = 10;
    static constexpr auto RATE_LIMIT_WINDOW = std::chrono::seconds{1};
    
    static constexpr size_t MAX_CONNECTIONS = 1000;
    // Reference the protocol constant directly — single source of truth.
    static constexpr size_t MAX_SESSION_ID_LENGTH = protocol::MAX_SESSION_ID_LENGTH;
    static constexpr auto MAX_BACKOFF = std::chrono::seconds{30};
    
    static constexpr int PLACEHOLDER_SEAT = -1;
    static constexpr double PLACEHOLDER_STACK = 0.0;
    
    // WebSocket stream idle/read timeouts.
    // Controls when Beast automatically closes connections with no activity.
    static constexpr auto WS_IDLE_TIMEOUT = std::chrono::hours{24};
    static constexpr auto WS_READ_TIMEOUT = std::chrono::hours{24};

    static constexpr int64_t MAX_SEQUENCE_GAP = 10000;
};

} // namespace server
} // namespace cppsim
