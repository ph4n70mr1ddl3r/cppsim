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
        
        config_path_ = path;
        last_reload_time_ = std::chrono::steady_clock::now();
        
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
    if (config_path_.empty()) {
        log_error("[RuntimeConfig] No config file loaded for reload");
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto time_since_reload = std::chrono::duration_cast<std::chrono::seconds>(now - last_reload_time_);
    
    if (time_since_reload < reload_interval_) {
        return true; // Not enough time has passed
    }
    
    return load_from_file(config_path_);
}

bool runtime_config_manager::load_from_json(const nlohmann::json& config_json) noexcept {
    try {
        // Store current config for rollback if validation fails
        auto old_max_connections = max_connections_;
        auto old_handshake_timeout = handshake_timeout_;
        auto old_max_message_size = max_message_size_;
        auto old_max_write_queue_size = max_write_queue_size_;
        auto old_max_messages_per_window = max_messages_per_window_;
        auto old_rate_limit_window = rate_limit_window_;
        auto old_max_backoff = max_backoff_;
        auto old_ws_idle_timeout = ws_idle_timeout_;
        auto old_ws_read_timeout = ws_read_timeout_;
        auto old_max_amount = max_amount_;
        auto old_max_sequence_gap = max_sequence_gap_;
        auto old_security_enabled = security_enabled_;
        auto old_metrics_enabled = metrics_enabled_;
        
        // Load values from JSON with validation
        if (config_json.contains("max_connections") && config_json["max_connections"].is_number()) {
            max_connections_ = config_json["max_connections"].get<int>();
            if (max_connections_ < 1 || max_connections_ > 10000) {
                log_error("[RuntimeConfig] Invalid max_connections, using default");
                max_connections_ = 1000;
            }
        }
        
        if (config_json.contains("handshake_timeout") && config_json["handshake_timeout"].is_number()) {
            handshake_timeout_ = std::chrono::seconds(config_json["handshake_timeout"].get<int>());
            if (handshake_timeout_ < std::chrono::seconds(1) || handshake_timeout_ > std::chrono::seconds(300)) {
                log_error("[RuntimeConfig] Invalid handshake_timeout, using default");
                handshake_timeout_ = std::chrono::seconds{10};
            }
        }
        
        if (config_json.contains("max_message_size") && config_json["max_message_size"].is_number()) {
            max_message_size_ = config_json["max_message_size"].get<size_t>();
            if (max_message_size_ < 1024 || max_message_size_ > 1024 * 1024) {
                log_error("[RuntimeConfig] Invalid max_message_size, using default");
                max_message_size_ = 64 * 1024;
            }
        }
        
        if (config_json.contains("max_write_queue_size") && config_json["max_write_queue_size"].is_number()) {
            max_write_queue_size_ = config_json["max_write_queue_size"].get<size_t>();
            if (max_write_queue_size_ < 10 || max_write_queue_size_ > 1000) {
                log_error("[RuntimeConfig] Invalid max_write_queue_size, using default");
                max_write_queue_size_ = 100;
            }
        }
        
        if (config_json.contains("max_messages_per_window") && config_json["max_messages_per_window"].is_number()) {
            max_messages_per_window_ = config_json["max_messages_per_window"].get<size_t>();
            if (max_messages_per_window_ < 1 || max_messages_per_window_ > 1000) {
                log_error("[RuntimeConfig] Invalid max_messages_per_window, using default");
                max_messages_per_window_ = 10;
            }
        }
        
        if (config_json.contains("rate_limit_window") && config_json["rate_limit_window"].is_number()) {
            rate_limit_window_ = std::chrono::seconds(config_json["rate_limit_window"].get<int>());
            if (rate_limit_window_ < std::chrono::seconds(1) || rate_limit_window_ > std::chrono::seconds(60)) {
                log_error("[RuntimeConfig] Invalid rate_limit_window, using default");
                rate_limit_window_ = std::chrono::seconds{1};
            }
        }
        
        if (config_json.contains("max_backoff") && config_json["max_backoff"].is_number()) {
            max_backoff_ = std::chrono::seconds(config_json["max_backoff"].get<int>());
            if (max_backoff_ < std::chrono::seconds(1) || max_backoff_ > std::chrono::seconds(300)) {
                log_error("[RuntimeConfig] Invalid max_backoff, using default");
                max_backoff_ = std::chrono::seconds{30};
            }
        }
        
        if (config_json.contains("ws_idle_timeout") && config_json["ws_idle_timeout"].is_number()) {
            ws_idle_timeout_ = std::chrono::seconds(config_json["ws_idle_timeout"].get<int>());
            if (ws_idle_timeout_ < std::chrono::seconds(1) || ws_idle_timeout_ > std::chrono::hours(48)) {
                log_error("[RuntimeConfig] Invalid ws_idle_timeout, using default");
                ws_idle_timeout_ = std::chrono::seconds{24 * 3600};
            }
        }
        
        if (config_json.contains("ws_read_timeout") && config_json["ws_read_timeout"].is_number()) {
            ws_read_timeout_ = std::chrono::seconds(config_json["ws_read_timeout"].get<int>());
            if (ws_read_timeout_ < std::chrono::seconds(1) || ws_read_timeout_ > std::chrono::hours(48)) {
                log_error("[RuntimeConfig] Invalid ws_read_timeout, using default");
                ws_read_timeout_ = std::chrono::seconds{24 * 3600};
            }
        }
        
        if (config_json.contains("max_amount") && config_json["max_amount"].is_number()) {
            max_amount_ = config_json["max_amount"].get<int64_t>();
            if (max_amount_ < 1000 || max_amount_ > 1000000000000000LL) {
                log_error("[RuntimeConfig] Invalid max_amount, using default");
                max_amount_ = 1000000000000000LL;
            }
        }
        
        if (config_json.contains("max_sequence_gap") && config_json["max_sequence_gap"].is_number()) {
            max_sequence_gap_ = config_json["max_sequence_gap"].get<int64_t>();
            if (max_sequence_gap_ < 100 || max_sequence_gap_ > 100000) {
                log_error("[RuntimeConfig] Invalid max_sequence_gap, using default");
                max_sequence_gap_ = 10000;
            }
        }
        
        if (config_json.contains("security_enabled") && config_json["security_enabled"].is_boolean()) {
            security_enabled_ = config_json["security_enabled"].get<bool>();
        }
        
        if (config_json.contains("metrics_enabled") && config_json["metrics_enabled"].is_boolean()) {
            metrics_enabled_ = config_json["metrics_enabled"].get<bool>();
        }
        
        if (!validate_config()) {
            // Validation failed, restore old config
            max_connections_ = old_max_connections;
            handshake_timeout_ = old_handshake_timeout;
            max_message_size_ = old_max_message_size;
            max_write_queue_size_ = old_max_write_queue_size;
            max_messages_per_window_ = old_max_messages_per_window;
            rate_limit_window_ = old_rate_limit_window;
            max_backoff_ = old_max_backoff;
            ws_idle_timeout_ = old_ws_idle_timeout;
            ws_read_timeout_ = old_ws_read_timeout;
            max_amount_ = old_max_amount;
            max_sequence_gap_ = old_max_sequence_gap;
            security_enabled_ = old_security_enabled;
            metrics_enabled_ = old_metrics_enabled;
            return false;
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
    try {
        // Validate configuration relationships that aren't checked per-field.
        // Individual field clamping is done in load_from_json(); this checks
        // cross-field invariants.
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