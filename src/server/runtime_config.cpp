#include "runtime_config.hpp"

#include <climits>
#include <fstream>
#include <stdexcept>
#include <system_error>

namespace cppsim {
namespace server {

bool runtime_config::config_loaded_ = false;
std::unordered_map<std::string, std::string> runtime_config::config_values_;

void runtime_config::load_from_env() {
    const std::unordered_map<std::string, std::string> env_mappings = {
        {"CPPSIM_HANDSHAKE_TIMEOUT", "handshake_timeout"},
        {"CPPSIM_IDLE_TIMEOUT", "idle_timeout"},
        {"CPPSIM_MAX_MESSAGE_SIZE", "max_message_size"},
        {"CPPSIM_DEFAULT_PORT", "default_port"},
        {"CPPSIM_MAX_CONNECTIONS", "max_connections"},
        {"CPPSIM_MAX_MESSAGES_PER_WINDOW", "max_messages_per_window"},
        {"CPPSIM_RATE_LIMIT_WINDOW", "rate_limit_window"},
        {"CPPSIM_MAX_BACKOFF", "max_backoff"},
        {"CPPSIM_MAX_SEQUENCE_GAP", "max_sequence_gap"}
    };
    
    for (const auto& [env_var, config_key] : env_mappings) {
        try {
            if (const char* value = std::getenv(env_var.c_str())) {
                config_values_[config_key] = value;
            }
        } catch (const std::exception&) {
            // Environment variable doesn't exist, ignore
        }
    }
    
    if (!config_values_.empty()) {
        config_loaded_ = true;
    }
}

bool runtime_config::load_from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        size_t equals_pos = line.find('=');
        if (equals_pos != std::string::npos) {
            std::string key = line.substr(0, equals_pos);
            std::string value = line.substr(equals_pos + 1);
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t\r\n"));
            key.erase(key.find_last_not_of(" \t\r\n") + 1);
            value.erase(0, value.find_first_not_of(" \t\r\n"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);
            
            config_values_[key] = value;
        }
    }
    
    if (!config_values_.empty()) {
        config_loaded_ = true;
        return true;
    }
    
    return false;
}

void runtime_config::set_value(const std::string& key, const std::string& value) {
    config_values_[key] = value;
    config_loaded_ = true;
}

std::optional<std::chrono::seconds> runtime_config::get_handshake_timeout() {
    if (auto it = config_values_.find("handshake_timeout"); it != config_values_.end()) {
        return parse_duration(it->second);
    }
    return std::nullopt;
}

std::optional<std::chrono::seconds> runtime_config::get_idle_timeout() {
    if (auto it = config_values_.find("idle_timeout"); it != config_values_.end()) {
        return parse_duration(it->second);
    }
    return std::nullopt;
}

std::optional<size_t> runtime_config::get_max_message_size() {
    if (auto it = config_values_.find("max_message_size"); it != config_values_.end()) {
        return parse_size_t(it->second);
    }
    return std::nullopt;
}

std::optional<unsigned short> runtime_config::get_default_port() {
    if (auto it = config_values_.find("default_port"); it != config_values_.end()) {
        return parse_ushort(it->second);
    }
    return std::nullopt;
}

std::optional<size_t> runtime_config::get_max_connections() {
    if (auto it = config_values_.find("max_connections"); it != config_values_.end()) {
        return parse_size_t(it->second);
    }
    return std::nullopt;
}

std::optional<size_t> runtime_config::get_max_messages_per_window() {
    if (auto it = config_values_.find("max_messages_per_window"); it != config_values_.end()) {
        return parse_size_t(it->second);
    }
    return std::nullopt;
}

std::optional<std::chrono::seconds> runtime_config::get_rate_limit_window() {
    if (auto it = config_values_.find("rate_limit_window"); it != config_values_.end()) {
        return parse_duration(it->second);
    }
    return std::nullopt;
}

std::optional<std::chrono::seconds> runtime_config::get_max_backoff() {
    if (auto it = config_values_.find("max_backoff"); it != config_values_.end()) {
        return parse_duration(it->second);
    }
    return std::nullopt;
}

std::optional<int64_t> runtime_config::get_max_sequence_gap() {
    if (auto it = config_values_.find("max_sequence_gap"); it != config_values_.end()) {
        return parse_int64_t(it->second);
    }
    return std::nullopt;
}

std::optional<std::chrono::seconds> runtime_config::parse_duration(const std::string& value) {
    try {
        size_t pos;
        long long seconds = std::stoll(value, &pos);
        if (pos == value.length() && seconds >= 0) {
            return std::chrono::seconds{static_cast<int64_t>(seconds)};
        }
    } catch (const std::exception&) {
        // Invalid format
    }
    return std::nullopt;
}

std::optional<size_t> runtime_config::parse_size_t(const std::string& value) {
    try {
        size_t pos;
        size_t result = std::stoul(value, &pos);
        if (pos == value.length()) {
            return result;
        }
    } catch (const std::exception&) {
        // Invalid format
    }
    return std::nullopt;
}

std::optional<unsigned short> runtime_config::parse_ushort(const std::string& value) {
    try {
        size_t pos;
        unsigned long result = std::stoul(value, &pos);
        if (pos == value.length() && result <= USHRT_MAX) {
            return static_cast<unsigned short>(result);
        }
    } catch (const std::exception&) {
        // Invalid format
    }
    return std::nullopt;
}

std::optional<int64_t> runtime_config::parse_int64_t(const std::string& value) {
    try {
        size_t pos;
        int64_t result = std::stoll(value, &pos);
        if (pos == value.length()) {
            return result;
        }
    } catch (const std::exception&) {
        // Invalid format
    }
    return std::nullopt;
}

} // namespace server
} // namespace cppsim