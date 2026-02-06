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
};

} // namespace server
} // namespace cppsim
