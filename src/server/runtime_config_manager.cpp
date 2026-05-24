#include "runtime_config_manager.hpp"

#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <ctime>

#include "logger.hpp"
#include "config.hpp" // For fallback defaults

namespace cppsim {
namespace server {

bool runtime_config_manager::load_from_file(const std::string& path) noexcept {
    try {
        // Parse the file outside the lock — I/O + JSON parsing don't need
        // mutual exclusion and holding the lock during disk reads would
        // block all reader threads unnecessarily.
        std::ifstream config_file(path);
        if (!config_file.is_open()) {
            log_error(std::string("[RuntimeConfig] Failed to open config file: ") + path);
            return false;
        }
        
        nlohmann::json config_json;
        try {
            config_file >> config_json;
        } catch (const nlohmann::json::exception& e) {
            log_error(std::string("[RuntimeConfig] JSON parse error: ") + e.what());
            return false;
        }
        
        if (!load_from_json(config_json)) {
            return false;
        }
        
        {
            std::lock_guard<std::mutex> lock(config_mutex_);
            config_path_ = path;
            last_reload_time_ = std::chrono::steady_clock::now();
        }
        
        log_message(std::string("[RuntimeConfig] Successfully loaded configuration from: ") + path);
        return true;
        
    } catch (const std::exception& e) {
        log_error(std::string("[RuntimeConfig] Exception loading config: ") + e.what());
        return false;
    } catch (...) {
        log_error("[RuntimeConfig] Unknown exception loading config");
        return false;
    }
}

bool runtime_config_manager::reload() noexcept {
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        if (config_path_.empty()) {
            log_error("[RuntimeConfig] No config file loaded for reload");
            return false;
        }
    }
    // load_from_file() takes its own lock internally.
    return load_from_file(get_config_path());
}

bool runtime_config_manager::load_from_json(const nlohmann::json& config_json) noexcept {
    try {
        // Build the new config outside the lock first, then swap under the
        // lock.  This minimizes the time readers are blocked.
        int new_max_connections = get_max_connections();
        auto new_handshake_timeout = get_handshake_timeout();
        size_t new_max_message_size = get_max_message_size();
        size_t new_max_write_queue_size = get_max_write_queue_size();
        size_t new_max_messages_per_window = get_max_messages_per_window();
        auto new_rate_limit_window = get_rate_limit_window();
        auto new_max_backoff = get_max_backoff();
        auto new_ws_idle_timeout = get_ws_idle_timeout();
        auto new_ws_read_timeout = get_ws_read_timeout();
        int64_t new_max_amount = get_max_amount();
        int64_t new_max_sequence_gap = get_max_sequence_gap();
        bool new_security_enabled = is_security_enabled();
        bool new_metrics_enabled = is_metrics_enabled();

        // Load values from JSON with per-field clamping
        if (config_json.contains("max_connections") && config_json["max_connections"].is_number()) {
            new_max_connections = config_json["max_connections"].get<int>();
            if (new_max_connections < 1 || new_max_connections > 10000) {
                log_error("[RuntimeConfig] Invalid max_connections, using default");
                new_max_connections = 1000;
            }
        }
        
        if (config_json.contains("handshake_timeout") && config_json["handshake_timeout"].is_number()) {
            new_handshake_timeout = std::chrono::seconds(config_json["handshake_timeout"].get<int>());
            if (new_handshake_timeout < std::chrono::seconds(1) || new_handshake_timeout > std::chrono::seconds(300)) {
                log_error("[RuntimeConfig] Invalid handshake_timeout, using default");
                new_handshake_timeout = std::chrono::seconds{10};
            }
        }
        
        if (config_json.contains("max_message_size") && config_json["max_message_size"].is_number()) {
            new_max_message_size = config_json["max_message_size"].get<size_t>();
            if (new_max_message_size < 1024 || new_max_message_size > 1024 * 1024) {
                log_error("[RuntimeConfig] Invalid max_message_size, using default");
                new_max_message_size = 64 * 1024;
            }
        }
        
        if (config_json.contains("max_write_queue_size") && config_json["max_write_queue_size"].is_number()) {
            new_max_write_queue_size = config_json["max_write_queue_size"].get<size_t>();
            if (new_max_write_queue_size < 10 || new_max_write_queue_size > 1000) {
                log_error("[RuntimeConfig] Invalid max_write_queue_size, using default");
                new_max_write_queue_size = 100;
            }
        }
        
        if (config_json.contains("max_messages_per_window") && config_json["max_messages_per_window"].is_number()) {
            new_max_messages_per_window = config_json["max_messages_per_window"].get<size_t>();
            if (new_max_messages_per_window < 1 || new_max_messages_per_window > 1000) {
                log_error("[RuntimeConfig] Invalid max_messages_per_window, using default");
                new_max_messages_per_window = 10;
            }
        }
        
        if (config_json.contains("rate_limit_window") && config_json["rate_limit_window"].is_number()) {
            new_rate_limit_window = std::chrono::seconds(config_json["rate_limit_window"].get<int>());
            if (new_rate_limit_window < std::chrono::seconds(1) || new_rate_limit_window > std::chrono::seconds(60)) {
                log_error("[RuntimeConfig] Invalid rate_limit_window, using default");
                new_rate_limit_window = std::chrono::seconds{1};
            }
        }
        
        if (config_json.contains("max_backoff") && config_json["max_backoff"].is_number()) {
            new_max_backoff = std::chrono::seconds(config_json["max_backoff"].get<int>());
            if (new_max_backoff < std::chrono::seconds(1) || new_max_backoff > std::chrono::seconds(300)) {
                log_error("[RuntimeConfig] Invalid max_backoff, using default");
                new_max_backoff = std::chrono::seconds{30};
            }
        }
        
        if (config_json.contains("ws_idle_timeout") && config_json["ws_idle_timeout"].is_number()) {
            new_ws_idle_timeout = std::chrono::seconds(config_json["ws_idle_timeout"].get<int>());
            if (new_ws_idle_timeout < std::chrono::seconds(1) || new_ws_idle_timeout > std::chrono::hours(48)) {
                log_error("[RuntimeConfig] Invalid ws_idle_timeout, using default");
                new_ws_idle_timeout = std::chrono::seconds{24 * 3600};
            }
        }
        
        if (config_json.contains("ws_read_timeout") && config_json["ws_read_timeout"].is_number()) {
            new_ws_read_timeout = std::chrono::seconds(config_json["ws_read_timeout"].get<int>());
            if (new_ws_read_timeout < std::chrono::seconds(1) || new_ws_read_timeout > std::chrono::hours(48)) {
                log_error("[RuntimeConfig] Invalid ws_read_timeout, using default");
                new_ws_read_timeout = std::chrono::seconds{24 * 3600};
            }
        }
        
        if (config_json.contains("max_amount") && config_json["max_amount"].is_number()) {
            new_max_amount = config_json["max_amount"].get<int64_t>();
            if (new_max_amount < 1000 || new_max_amount > 1000000000000000LL) {
                log_error("[RuntimeConfig] Invalid max_amount, using default");
                new_max_amount = 1000000000000000LL;
            }
        }
        
        if (config_json.contains("max_sequence_gap") && config_json["max_sequence_gap"].is_number()) {
            new_max_sequence_gap = config_json["max_sequence_gap"].get<int64_t>();
            if (new_max_sequence_gap < 100 || new_max_sequence_gap > 100000) {
                log_error("[RuntimeConfig] Invalid max_sequence_gap, using default");
                new_max_sequence_gap = 10000;
            }
        }
        
        if (config_json.contains("security_enabled") && config_json["security_enabled"].is_boolean()) {
            new_security_enabled = config_json["security_enabled"].get<bool>();
        }
        
        if (config_json.contains("metrics_enabled") && config_json["metrics_enabled"].is_boolean()) {
            new_metrics_enabled = config_json["metrics_enabled"].get<bool>();
        }
        
        // Validate cross-field invariants before committing
        if (new_max_write_queue_size < new_max_messages_per_window) {
            log_error("[RuntimeConfig] max_write_queue_size must be >= max_messages_per_window");
            return false;
        }
        
        // Commit the new values under a single lock acquisition
        {
            std::lock_guard<std::mutex> lock(config_mutex_);
            max_connections_ = new_max_connections;
            handshake_timeout_ = new_handshake_timeout;
            max_message_size_ = new_max_message_size;
            max_write_queue_size_ = new_max_write_queue_size;
            max_messages_per_window_ = new_max_messages_per_window;
            rate_limit_window_ = new_rate_limit_window;
            max_backoff_ = new_max_backoff;
            ws_idle_timeout_ = new_ws_idle_timeout;
            ws_read_timeout_ = new_ws_read_timeout;
            max_amount_ = new_max_amount;
            max_sequence_gap_ = new_max_sequence_gap;
            security_enabled_ = new_security_enabled;
            metrics_enabled_ = new_metrics_enabled;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        log_error(std::string("[RuntimeConfig] Exception loading JSON: ") + e.what());
        return false;
    } catch (...) {
        log_error("[RuntimeConfig] Unknown exception loading JSON");
        return false;
    }
}

bool runtime_config_manager::validate_config() noexcept {
    // validate_config() is no longer called from load_from_json() (validation
    // is done inline there now).  Kept for potential future external callers.
    try {
        std::lock_guard<std::mutex> lock(config_mutex_);
        if (max_write_queue_size_ < max_messages_per_window_) {
            log_error("[RuntimeConfig] max_write_queue_size must be >= max_messages_per_window");
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        log_error(std::string("[RuntimeConfig] Exception during validation: ") + e.what());
        return false;
    } catch (...) {
        log_error("[RuntimeConfig] Unknown exception during validation");
        return false;
    }
}

void runtime_config_manager::set_defaults() noexcept {
    std::lock_guard<std::mutex> lock(config_mutex_);
    max_connections_ = 1000;
    handshake_timeout_ = std::chrono::seconds{10};
    max_message_size_ = 64 * 1024;
    max_write_queue_size_ = 100;
    max_messages_per_window_ = 10;
    rate_limit_window_ = std::chrono::seconds{1};
    max_backoff_ = std::chrono::seconds{30};
    ws_idle_timeout_ = std::chrono::hours{24};
    ws_read_timeout_ = std::chrono::hours{24};
    max_amount_ = 1000000000000000LL;
    max_sequence_gap_ = 10000;
    security_enabled_ = true;
    metrics_enabled_ = true;
    reload_interval_ = std::chrono::seconds{5};
}

std::string runtime_config_manager::export_to_json() const noexcept {
    try {
        nlohmann::json config_json;
        {
            std::lock_guard<std::mutex> lock(config_mutex_);
            config_json["max_connections"] = max_connections_;
            config_json["handshake_timeout"] = handshake_timeout_.count();
            config_json["max_message_size"] = max_message_size_;
            config_json["max_write_queue_size"] = max_write_queue_size_;
            config_json["max_messages_per_window"] = max_messages_per_window_;
            config_json["rate_limit_window"] = rate_limit_window_.count();
            config_json["max_backoff"] = max_backoff_.count();
            config_json["ws_idle_timeout"] = ws_idle_timeout_.count();
            config_json["ws_read_timeout"] = ws_read_timeout_.count();
            config_json["max_amount"] = max_amount_;
            config_json["max_sequence_gap"] = max_sequence_gap_;
            config_json["security_enabled"] = security_enabled_;
            config_json["metrics_enabled"] = metrics_enabled_;
            config_json["config_path"] = config_path_;
        }
        config_json["last_reload_time"] = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());
        
        return config_json.dump(2);
        
    } catch (const std::exception& e) {
        log_error(std::string("[RuntimeConfig] Exception exporting JSON: ") + e.what());
        return "{}";
    } catch (...) {
        log_error("[RuntimeConfig] Unknown exception exporting JSON");
        return "{}";
    }
}

} // namespace server
} // namespace cppsim
