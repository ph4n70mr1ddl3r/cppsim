#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

#include "config.hpp"

namespace cppsim {
namespace server {

// Runtime configuration loader
// Provides capability to override config constants at runtime
class runtime_config {
public:
    // Load configuration from environment variables
    static void load_from_env();
    
    // Load configuration from a simple key=value format file
    static bool load_from_file(const std::string& filepath);
    
    // Set a configuration value manually
    static void set_value(const std::string& key, const std::string& value);
    
    // Get configuration values with type-safe access
    static std::optional<std::chrono::seconds> get_handshake_timeout();
    static std::optional<std::chrono::seconds> get_idle_timeout();
    static std::optional<size_t> get_max_message_size();
    static std::optional<unsigned short> get_default_port();
    static std::optional<size_t> get_max_connections();
    static std::optional<size_t> get_max_messages_per_window();
    static std::optional<std::chrono::seconds> get_rate_limit_window();
    static std::optional<std::chrono::seconds> get_max_backoff();
    static std::optional<int64_t> get_max_sequence_gap();
    
    // Check if any configuration was loaded
    static bool is_config_loaded() { return config_loaded_; }
    
private:
    static bool config_loaded_;
    static std::unordered_map<std::string, std::string> config_values_;
    
    // Helper to parse time durations
    static std::optional<std::chrono::seconds> parse_duration(const std::string& value);
    
    // Helper to parse size_t
    static std::optional<size_t> parse_size_t(const std::string& value);
    
    // Helper to parse unsigned short
    static std::optional<unsigned short> parse_ushort(const std::string& value);
    
    // Helper to parse int64_t
    static std::optional<int64_t> parse_int64_t(const std::string& value);
};

// Enhanced config that can use runtime configuration
struct enhanced_config {
    // Use runtime config if available, otherwise fall back to compile-time constants
    static auto get_handshake_timeout() {
        if (auto timeout = runtime_config::get_handshake_timeout()) {
            return *timeout;
        }
        return config::HANDSHAKE_TIMEOUT;
    }
    
    static auto get_idle_timeout() {
        if (auto timeout = runtime_config::get_idle_timeout()) {
            return *timeout;
        }
        return config::IDLE_TIMEOUT;
    }
    
    static size_t get_max_message_size() {
        if (auto size = runtime_config::get_max_message_size()) {
            return *size;
        }
        return config::MAX_MESSAGE_SIZE;
    }
    
    static unsigned short get_default_port() {
        if (auto port = runtime_config::get_default_port()) {
            return *port;
        }
        return config::DEFAULT_PORT;
    }
    
    static size_t get_max_connections() {
        if (auto max = runtime_config::get_max_connections()) {
            return *max;
        }
        return config::MAX_CONNECTIONS;
    }
    
    static size_t get_max_messages_per_window() {
        if (auto max = runtime_config::get_max_messages_per_window()) {
            return *max;
        }
        return config::MAX_MESSAGES_PER_WINDOW;
    }
    
    static auto get_rate_limit_window() {
        if (auto window = runtime_config::get_rate_limit_window()) {
            return *window;
        }
        return config::RATE_LIMIT_WINDOW;
    }
    
    static auto get_max_backoff() {
        if (auto backoff = runtime_config::get_max_backoff()) {
            return *backoff;
        }
        return config::MAX_BACKOFF;
    }
    
    static int64_t get_max_sequence_gap() {
        if (auto gap = runtime_config::get_max_sequence_gap()) {
            return *gap;
        }
        return config::MAX_SEQUENCE_GAP;
    }
};

} // namespace server
} // namespace cppsim