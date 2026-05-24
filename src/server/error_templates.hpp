#pragma once

#include <string>
#include <string_view>

namespace cppsim {
namespace server {

namespace error_templates {
    constexpr std::string_view INSUFFICIENT_FUNDS = 
        "Insufficient funds for action. Required: {}, Available: {}";
    constexpr std::string_view INVALID_ACTION = 
        "Invalid action '{}' for current game state";
    constexpr std::string_view RATE_LIMITED = 
        "Rate limit exceeded. Please wait before sending more messages";
    constexpr std::string_view INVALID_SEQUENCE = 
        "Invalid sequence number. Expected: {}, Got: {}";
    constexpr std::string_view AUTH_REQUIRED = 
        "Authentication required. Please send handshake message first";
    constexpr std::string_view SESSION_EXPIRED = 
        "Session expired. Please reconnect to establish new session";
    constexpr std::string_view MALFORMED_JSON = 
        "Invalid JSON format: {}";
    constexpr std::string_view INVALID_AMOUNT = 
        "Invalid amount: {}. Must be between {} and {}";
}

} // namespace server
} // namespace cppsim