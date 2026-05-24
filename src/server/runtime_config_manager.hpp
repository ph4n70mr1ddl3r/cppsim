#pragma once

#include <string>
#include <chrono>
#include <cstdint>
#include <optional>
#include <fstream>
#include <nlohmann/json.hpp>

namespace cppsim {
namespace server {

/**
 * @brief Runtime configuration manager for cppsim poker server
 * 
 * Provides dynamic configuration loading and validation with hot-reload capability.
 * All configuration values have sensible defaults and validation.
 */
class runtime_config_manager {
public:
    // Delete copy constructor and assignment operator
    runtime_config_manager(const runtime_config_manager&) = delete;
    runtime_config_manager& operator=(const runtime_config_manager&) = delete;
    
    // Allow move semantics
    runtime_config_manager(runtime_config_manager&&) = default;
    runtime_config_manager& operator=(runtime_config_manager&&) = default;
    
    static runtime_config_manager& instance() {
        static runtime_config_manager instance;
        return instance;
    }
    
    /**
     * @brief Load configuration from JSON file
     * @param path Path to configuration file
     * @return true if successful, false otherwise
     */
    bool load_from_file(const std::string& path) noexcept;
    
    /**
     * @brief Reload current configuration (hot-reload)
     * @return true if successful, false otherwise
     */
    bool reload() noexcept;
    
    /**
     * @brief Get maximum number of concurrent connections
     */
    [[nodiscard]] int get_max_connections() const noexcept {
        return max_connections_;
    }
    
    /**
     * @brief Get handshake timeout duration
     */
    [[nodiscard]] std::chrono::seconds get_handshake_timeout() const noexcept {
        return handshake_timeout_;
    }
    
    /**
     * @brief Get maximum message size in bytes
     */
    [[nodiscard]] size_t get_max_message_size() const noexcept {
        return max_message_size_;
    }
    
    /**
     * @brief Get maximum write queue size
     */
    [[nodiscard]] size_t get_max_write_queue_size() const noexcept {
        return max_write_queue_size_;
    }
    
    /**
     * @brief Get maximum messages per rate limit window
     */
    [[nodiscard]] size_t get_max_messages_per_window() const noexcept {
        return max_messages_per_window_;
    }
    
    /**
     * @brief Get rate limit window duration
     */
    [[nodiscard]] std::chrono::seconds get_rate_limit_window() const noexcept {
        return rate_limit_window_;
    }
    
    /**
     * @brief Get maximum backoff duration
     */
    [[nodiscard]] std::chrono::seconds get_max_backoff() const noexcept {
        return max_backoff_;
    }
    
    /**
     * @brief Get WebSocket idle timeout
     */
    [[nodiscard]] std::chrono::seconds get_ws_idle_timeout() const noexcept {
        return ws_idle_timeout_;
    }
    
    /**
     * @brief Get WebSocket read timeout
     */
    [[nodiscard]] std::chrono::seconds get_ws_read_timeout() const noexcept {
        return ws_read_timeout_;
    }
    
    /**
     * @brief Get maximum allowed amount
     */
    [[nodiscard]] int64_t get_max_amount() const noexcept {
        return max_amount_;
    }
    
    /**
     * @brief Get maximum sequence number gap
     */
    [[nodiscard]] int64_t get_max_sequence_gap() const noexcept {
        return max_sequence_gap_;
    }
    
    /**
     * @brief Get security enable flag
     */
    [[nodiscard]] bool is_security_enabled() const noexcept {
        return security_enabled_;
    }
    
    /**
     * @brief Get whether to enable metrics collection
     */
    [[nodiscard]] bool is_metrics_enabled() const noexcept {
        return metrics_enabled_;
    }
    
    /**
     * @brief Get configuration file path
     */
    [[nodiscard]] std::string get_config_path() const noexcept {
        return config_path_;
    }
    
    /**
     * @brief Export current configuration to JSON string
     */
    [[nodiscard]] std::string export_to_json() const noexcept;

private:
    runtime_config_manager() = default;
    ~runtime_config_manager() = default;
    
    /**
     * @brief Validate loaded configuration values
     * @return true if valid, false otherwise
     */
    bool validate_config() noexcept;
    
    /**
     * @brief Set default configuration values
     */
    void set_defaults() noexcept;
    
    /**
     * @brief Load configuration from JSON object
     * @param config_json JSON configuration object
     * @return true if successful, false otherwise
     */
    bool load_from_json(const nlohmann::json& config_json) noexcept;
    
    // Configuration members with defaults
    std::string config_path_;
    int max_connections_{1000};
    std::chrono::seconds handshake_timeout_{std::chrono::seconds{10}};
    size_t max_message_size_{64 * 1024};
    size_t max_write_queue_size_{100};
    size_t max_messages_per_window_{10};
    std::chrono::seconds rate_limit_window_{std::chrono::seconds{1}};
    std::chrono::seconds max_backoff_{std::chrono::seconds{30}};
    std::chrono::seconds ws_idle_timeout_{std::chrono::seconds{24 * 3600}};
    std::chrono::seconds ws_read_timeout_{std::chrono::seconds{24 * 3600}};
    int64_t max_amount_{1000000000000000LL};
    int64_t max_sequence_gap_{10000};
    bool security_enabled_{true};
    bool metrics_enabled_{true};
    
    // Hot-reload support
    std::chrono::steady_clock::time_point last_reload_time_;
    std::chrono::seconds reload_interval_{std::chrono::seconds{5}};
};

// Convenience functions
[[nodiscard]] inline int get_max_connections() noexcept {
    return runtime_config_manager::instance().get_max_connections();
}

[[nodiscard]] inline std::chrono::seconds get_handshake_timeout() noexcept {
    return runtime_config_manager::instance().get_handshake_timeout();
}

[[nodiscard]] inline size_t get_max_message_size() noexcept {
    return runtime_config_manager::instance().get_max_message_size();
}

[[nodiscard]] inline size_t get_max_write_queue_size() noexcept {
    return runtime_config_manager::instance().get_max_write_queue_size();
}

[[nodiscard]] inline size_t get_max_messages_per_window() noexcept {
    return runtime_config_manager::instance().get_max_messages_per_window();
}

[[nodiscard]] inline std::chrono::seconds get_rate_limit_window() noexcept {
    return runtime_config_manager::instance().get_rate_limit_window();
}

[[nodiscard]] inline std::chrono::seconds get_max_backoff() noexcept {
    return runtime_config_manager::instance().get_max_backoff();
}

[[nodiscard]] inline std::chrono::seconds get_ws_idle_timeout() noexcept {
    return runtime_config_manager::instance().get_ws_idle_timeout();
}

[[nodiscard]] inline std::chrono::seconds get_ws_read_timeout() noexcept {
    return runtime_config_manager::instance().get_ws_read_timeout();
}

[[nodiscard]] inline int64_t get_max_amount() noexcept {
    return runtime_config_manager::instance().get_max_amount();
}

[[nodiscard]] inline int64_t get_max_sequence_gap() noexcept {
    return runtime_config_manager::instance().get_max_sequence_gap();
}

[[nodiscard]] inline bool is_security_enabled() noexcept {
    return runtime_config_manager::instance().is_security_enabled();
}

[[nodiscard]] inline bool is_metrics_enabled() noexcept {
    return runtime_config_manager::instance().is_metrics_enabled();
}

} // namespace server
} // namespace cppsim