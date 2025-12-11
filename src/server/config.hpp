#pragma once

#include <chrono>

namespace cppsim {
namespace server {

// Server configuration constants
struct config {
    static constexpr auto HANDSHAKE_TIMEOUT = std::chrono::seconds(10);
    static constexpr auto IDLE_TIMEOUT = std::chrono::seconds(60);
    static constexpr size_t MAX_MESSAGE_SIZE = 64 * 1024; // 64KB
};

} // namespace server
} // namespace cppsim
