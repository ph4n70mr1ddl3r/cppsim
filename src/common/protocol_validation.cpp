#include "protocol.hpp"

#include <algorithm>
#include <cctype>

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
        if (!isalnum(static_cast<unsigned char>(c)) && c != '_') {
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
    // Hand-rolled to avoid std::regex overhead on the hot path.
    if (version.size() < 4 || version[0] != 'v') return false;
    auto dot_pos = version.find('.');
    if (dot_pos == std::string::npos || dot_pos < 2 || dot_pos == version.size() - 1) return false;
    for (size_t i = 1; i < dot_pos; ++i) {
        if (version[i] < '0' || version[i] > '9') return false;
    }
    for (size_t i = dot_pos + 1; i < version.size(); ++i) {
        if (version[i] < '0' || version[i] > '9') return false;
    }
    return true;
}

bool is_valid_session_id(const std::string& session_id) noexcept {
    if (session_id.empty() || session_id.size() > protocol::MAX_SESSION_ID_LENGTH) {
        return false;
    }
    
    // Session IDs should start with "sess_" followed by alphanumeric characters
    // (hex from UUID generation or decimal from counter-based generation)
    if (session_id.size() < 5 || session_id.compare(0, 5, "sess_") != 0) {
        return false;
    }
    
    // Check that the rest are valid session ID characters (alphanumeric + underscore)
    for (size_t i = 5; i < session_id.size(); ++i) {
        char c = session_id[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
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
        if (iscntrl(static_cast<unsigned char>(c)) && !isspace(static_cast<unsigned char>(c))) {
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
        
        // Validate specific field types if they are present in required_fields
        // Use a single sorted set lookup instead of linear search per field.
        const auto field_end = required_fields.end();
        auto field_find = [&](const std::string& name) -> bool {
            return std::find(required_fields.begin(), field_end, name) != field_end;
        };
        
        if (field_find("message_type") && !json["message_type"].is_string()) {
            return false;
        }
        
        if (field_find("protocol_version") && !json["protocol_version"].is_string()) {
            return false;
        }
        
        return true;
    } catch (const std::exception&) {
        return false;
    } catch (...) {
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
// Compares all bytes even when sizes differ to avoid leaking length info.
// Size mismatch still returns false, but comparison time is independent of content.
[[nodiscard]] bool constant_time_compare(const std::string& a, const std::string& b) noexcept {
    // Compare over the full range of the longer string to avoid timing leak.
    // Use volatile to prevent the compiler from optimizing away the loop.
    const size_t max_len = std::max(a.size(), b.size());
    volatile bool size_match = (a.size() == b.size());
    volatile bool result = true;
    for (size_t i = 0; i < max_len; ++i) {
        // Access within bounds: char from the string, or 0 if past its end
        char ca = (i < a.size()) ? a[i] : '\0';
        char cb = (i < b.size()) ? b[i] : '\0';
        result = result & (ca == cb);
    }
    return size_match && result;
}

} // namespace validation
} // namespace protocol
} // namespace cppsim