#pragma once

#include <string>
#include <string_view>
#include <charconv>

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

/**
 * @brief Format error message with arguments
 * @param template_str Error message template with {} placeholders
 * @param args Arguments to substitute
 * @return Formatted error message
 * 
 * NOTE: This is a placeholder implementation. The variadic template expansion
 * does not correctly substitute arguments one-by-one. For production use,
 * consider std::format (C++20) or fmtlib. Currently unused in production code.
 */
template<typename... Args>
[[nodiscard]] std::string format_error(std::string_view template_str, Args... args) {
    // Suppress unused parameter warnings
    (void)template_str;
    (void)(..., static_cast<void>(args));
    
    try {
        return std::string(template_str);
    } catch (...) {
        return std::string(template_str);
    }
}

} // namespace server
} // namespace cppsim