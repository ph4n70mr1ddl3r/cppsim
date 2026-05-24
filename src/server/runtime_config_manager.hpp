#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace cppsim {
namespace server {

/**
 * @brief Runtime configuration manager for cppsim poker server
 * 
 * Provides dynamic configuration loading and validation with hot-reload capability.
 * All configuration values have sensible defaults and validation.
 *
 * Thread Safety: All public methods are safe to call from any thread.
 * A single mutex (config_mutex_) protects all config members.
 */
class runtime_config_manager {
public:
    runtime_config_manager(const runtime_config_manager&) = delete;
    runtime_config_manager& operator=(const runtime_config_manager&) = delete;
    runtime_config_manager(runtime_config_manager&&) = delete;
    runtime_config_manager& operator=(runtime_config_manager&&) = delete;
    
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
    
    // Accessors — each takes config_mutex_ to prevent torn reads during reload.

    [[nodiscard]] int get_max_connections() const noexcept {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return max_connections_;
    }
    
    [[nodiscard]] std::chrono::seconds get_handshake_timeout() const noexcept {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return handshake_timeout_;
    }
    
    [[nodiscard]] size_t get_max_message_size() const noexcept {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return max_message_size_;
    }
    
    [[nodiscard]] size_t get_max_write_queue_size() const noexcept {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return max_write_queue_size_;
    }
    
    [[nodiscard]] size_t get_max_messages_per_window() const noexcept {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return max_messages_per_window_;
    }
    
    [[nodiscard]] std::chrono::seconds get_rate_limit_window() const noexcept {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return rate_limit_window_;
    }
    
    [[nodiscard]] std::chrono::seconds get_max_backoff() const noexcept {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return max_backoff_;
    }
    
    [[nodiscard]] std::chrono::seconds get_ws_idle_timeout() const noexcept {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return ws_idle_timeout_;
    }
    
    [[nodiscard]] std::chrono::seconds get_ws_read_timeout() const noexcept {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return ws_read_timeout_;
    }
    
    [[nodiscard]] int64_t get_max_amount() const noexcept {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return max_amount_;
    }
    
    [[nodiscard]] int64_t get_max_sequence_gap() const noexcept {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return max_sequence_gap_;
    }
    
    [[nodiscard]] bool is_security_enabled() const noexcept {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return security_enabled_;
    }
    
    [[nodiscard]] bool is_metrics_enabled() const noexcept {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return metrics_enabled_;
    }
    
    [[nodiscard]] std::string get_config_path() const noexcept {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return config_path_;
    }
    
    [[nodiscard]] std::string export_to_json() const noexcept;

private:
    runtime_config_manager() = default;
    ~runtime_config_manager() = default;
    
    bool validate_config() noexcept;
    void set_defaults() noexcept;
    bool load_from_json(const nlohmann::json& config_json) noexcept;
    
    mutable std::mutex config_mutex_;

    // Configuration members with defaults — all guarded by config_mutex_.
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

} // namespace server
} // namespace cppsim
