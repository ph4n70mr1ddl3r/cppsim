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
    
    // Session IDs should start with "sess_" followed by hexadecimal or
    // alphanumeric characters (hex from mixed generation, digits from
    // fallback counter-based generation).
    if (session_id.size() < 5 || session_id.compare(0, 5, "sess_") != 0) {
        return false;
    }
    
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

} // namespace validation
} // namespace protocol
} // namespace cppsim