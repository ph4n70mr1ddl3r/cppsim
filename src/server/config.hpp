#pragma once

#include <chrono>

namespace cppsim {
namespace server {

// Server configuration constants
struct config {
    static constexpr auto HANDSHAKE_TIMEOUT = std::chrono::seconds(10);
    static constexpr auto IDLE_TIMEOUT = std::chrono::seconds(60);
    static constexpr size_t MAX_MESSAGE_SIZE = 64 * 1024; // 64KB
    static constexpr size_t MAX_WRITE_QUEUE_SIZE = 100;
    static constexpr unsigned short DEFAULT_PORT = 8080;
    static constexpr unsigned short DEFAULT_TEST_PORT = 18080; // Test port to avoid conflicts
    static constexpr int MAX_MESSAGES_PER_SECOND = 10;
    static constexpr int PLACEHOLDER_SEAT = -1;
    static constexpr double PLACEHOLDER_STACK = 0.0;
    static constexpr size_t MAX_CONNECTIONS = 1000;
    static constexpr size_t MAX_SESSION_ID_LENGTH = 256;
    static constexpr size_t MAX_TIMESTAMPS_TO_TRACK = 50;
};

} // namespace server
} // namespace cppsim
