#include "protocol.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace cppsim {
namespace protocol {
namespace validation {

bool is_valid_message_type(const std::string& message_type,
                          const std::vector<std::string>& allowed_types) noexcept {
    if (message_type.empty()) {
        return false;
    }
    
    // Check for control characters and ensure it's alphanumeric with underscores
    for (char c : message_type) {
        if (!isalnum(c) && c != '_') {
            return false;
        }
    }
    
    // Check against allowed types
    return std::find(allowed_types.begin(), allowed_types.end(), message_type) != allowed_types.end();
}

bool is_valid_protocol_version(const std::string& version) noexcept {
    if (version.empty()) {
        return false;
    }
    
    // Expected format: v{major}.{minor} where major and minor are digits
    static const std::regex version_regex(R"(^v(\d+)\.(\d+)$)");
    return std::regex_match(version, version_regex);
}

bool is_valid_session_id(const std::string& session_id) noexcept {
    if (session_id.empty() || session_id.size() > protocol::MAX_SESSION_ID_LENGTH) {
        return false;
    }
    
    // Session IDs should start with "sess_" followed by hex characters
    if (session_id.size() < 5 || session_id.substr(0, 5) != "sess_") {
        return false;
    }
    
    // Check that the rest are valid hex characters
    std::string hex_part = session_id.substr(5);
    for (char c : hex_part) {
        if (!isxdigit(c)) {
            return false;
        }
    }
    
    return true;
}

bool is_valid_action_type(const std::string& action_type) noexcept {
    static const std::vector<std::string> valid_actions = {
        "FOLD", "CHECK", "CALL", "RAISE", "ALL_IN"
    };
    
    return std::find(valid_actions.begin(), valid_actions.end(), action_type) != valid_actions.end();
}

bool is_valid_amount(int64_t amount,
                     std::optional<int64_t> min_amount,
                     std::optional<int64_t> max_amount) noexcept {
    if (amount < 0) {
        return false;
    }
    
    if (min_amount.has_value() && amount < min_amount.value()) {
        return false;
    }
    
    if (max_amount.has_value() && amount > max_amount.value()) {
        return false;
    }
    
    // Check against absolute maximum
    if (amount > protocol::MAX_AMOUNT) {
        return false;
    }
    
    return true;
}

bool is_valid_game_phase(const std::string& game_phase) noexcept {
    static const std::vector<std::string> valid_phases = {
        "WAITING", "PREFLOP", "FLOP", "TURN", "RIVER", 
        "SHOWDOWN", "HAND_COMPLETE"
    };
    
    return std::find(valid_phases.begin(), valid_phases.end(), game_phase) != valid_phases.end();
}

bool is_valid_seat_number(int seat, int max_seats) noexcept {
    return seat >= 0 && seat < max_seats;
}

std::string sanitize_input(const std::string& input) noexcept {
    std::string result;
    result.reserve(input.size());
    
    for (char c : input) {
        // Remove control characters except for common whitespace
        if (iscntrl(c) && !isspace(c)) {
            continue;
        }
        
        // Escape quotes for JSON safety
        if (c == '\"' || c == '\\') {
            result += '\\';
        }
        
        result += c;
    }
    
    return result;
}

bool validate_message_envelope(const std::string& json_str,
                             const std::vector<std::string>& required_fields) noexcept {
    try {
        if (json_str.empty()) {
            return false;
        }
        
        auto json = nlohmann::json::parse(json_str);
        
        // Check if it's a JSON object
        if (!json.is_object()) {
            return false;
        }
        
        // Check all required fields exist
        for (const auto& field : required_fields) {
            if (!json.contains(field) || json[field].is_null()) {
                return false;
            }
        }
        
        // Validate message type field if it's required
        if (std::find(required_fields.begin(), required_fields.end(), "message_type") != required_fields.end()) {
            if (!json["message_type"].is_string()) {
                return false;
            }
        }
        
        // Validate protocol version field if it's required
        if (std::find(required_fields.begin(), required_fields.end(), "protocol_version") != required_fields.end()) {
            if (!json["protocol_version"].is_string()) {
                return false;
            }
        }
        
        return true;
    } catch (const nlohmann::json::exception&) {
        // JSON parsing failed
        return false;
    } catch (...) {
        // Any other exception
        return false;
    }
}

bool validate_sequence_gap(int64_t current, int64_t previous, int64_t max_gap) noexcept {
    if (current < 0 || previous < 0) {
        return false;
    }
    
    // Check for replay attacks (old sequence numbers)
    if (current <= previous) {
        return false;
    }
    
    // Check for gaps that are too large
    if (current - previous > max_gap) {
        return false;
    }
    
    return true;
}

// Constant-time comparison to prevent timing attacks
[[nodiscard]] bool constant_time_compare(const std::string& a, const std::string& b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    
    bool result = true;
    for (size_t i = 0; i < a.size(); ++i) {
        result &= (a[i] == b[i]);
    }
    
    return result;
}

} // namespace validation
} // namespace protocol
} // namespace cppsim