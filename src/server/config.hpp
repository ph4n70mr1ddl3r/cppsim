#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace cppsim {
namespace server {

// Server configuration constants
// Using inline constexpr for proper ODR compliance with non-integral types in C++17
struct config {
    // Timeouts
    static inline constexpr auto HANDSHAKE_TIMEOUT = std::chrono::seconds(10);  // Max time for client to send HANDSHAKE
    static inline constexpr auto IDLE_TIMEOUT = std::chrono::seconds(60);       // Disconnect after inactivity
    
    // Message limits
    static inline constexpr size_t MAX_MESSAGE_SIZE = 64 * 1024;                // 64KB max WebSocket message
    static inline constexpr size_t MAX_WRITE_QUEUE_SIZE = 100;                  // Max queued outgoing messages per session
    
    // Network
    static inline constexpr unsigned short DEFAULT_PORT = 8080;                 // Production server port
    static inline constexpr unsigned short DEFAULT_TEST_PORT = 18080;           // Test server port (avoid conflicts)
    
    // Rate limiting
    static inline constexpr size_t MAX_MESSAGES_PER_WINDOW = 10;                // Max messages per rate limit window
    static inline constexpr std::chrono::milliseconds RATE_LIMIT_WINDOW{1000};  // 1 second sliding window
    static inline constexpr size_t MAX_TIMESTAMPS_TO_TRACK = 50;                // Memory cap for rate limit tracking
    
    // Connection limits
    static inline constexpr size_t MAX_CONNECTIONS = 1000;                      // Max concurrent connections
    static inline constexpr size_t MAX_SESSION_ID_LENGTH = 256;                 // Max characters in session ID
    static inline constexpr int MAX_BACKOFF_SECONDS = 30;                       // Max retry backoff on accept errors
    
    // Game placeholders (will be replaced with real game engine values)
    static inline constexpr int PLACEHOLDER_SEAT = -1;
    static inline constexpr double PLACEHOLDER_STACK = 0.0;
    
    // Sequence number validation
    static inline constexpr int64_t MAX_SEQUENCE_GAP = 10000;  // Max allowed gap in sequence numbers (replay attack prevention)
    
    // Compile-time validation
    static_assert(MAX_TIMESTAMPS_TO_TRACK >= MAX_MESSAGES_PER_WINDOW,
                  "MAX_TIMESTAMPS_TO_TRACK must be >= MAX_MESSAGES_PER_WINDOW for rate limiting to work correctly");
};

} // namespace server
} // namespace cppsim
