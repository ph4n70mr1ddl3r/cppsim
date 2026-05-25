#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

#include "config.hpp"
#include "runtime_config_manager.hpp"

namespace cppsim {
namespace server {

// Enhanced config adapter that reads from runtime_config_manager
// (the active JSON-based config system) and falls back to compile-time constants.
struct enhanced_config {
    // Use runtime config if available, otherwise fall back to compile-time constants
    static auto get_handshake_timeout() {
        auto timeout = runtime_config_manager::instance().get_handshake_timeout();
        return timeout;
    }
    
    static auto get_idle_timeout() {
        return runtime_config_manager::instance().get_ws_idle_timeout();
    }
    
    static size_t get_max_message_size() {
        return runtime_config_manager::instance().get_max_message_size();
    }
    
    static unsigned short get_default_port() {
        // runtime_config_manager doesn't expose default_port;
        // fall back to the compile-time constant.
        return config::DEFAULT_PORT;
    }
    
    static size_t get_max_connections() {
        return static_cast<size_t>(runtime_config_manager::instance().get_max_connections());
    }
    
    static size_t get_max_messages_per_window() {
        return runtime_config_manager::instance().get_max_messages_per_window();
    }
    
    static auto get_rate_limit_window() {
        return runtime_config_manager::instance().get_rate_limit_window();
    }
    
    static auto get_max_backoff() {
        return runtime_config_manager::instance().get_max_backoff();
    }
    
    static int64_t get_max_sequence_gap() {
        return runtime_config_manager::instance().get_max_sequence_gap();
    }
};

} // namespace server
} // namespace cppsim