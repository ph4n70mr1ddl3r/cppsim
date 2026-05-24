#pragma once

#include <string>
#include <string_view>

namespace cppsim {
namespace server {

// Error message templates for structured error formatting.
// Placeholders use {0}, {1}, etc. for positional arguments.
// These are intended for future use with a fmt-style formatter.
// Currently they serve as documentation of intended error messages.
namespace error_templates {
    constexpr std::string_view INSUFFICIENT_FUNDS =
        "Insufficient funds for action. Required: {0}, Available: {1}";
    constexpr std::string_view INVALID_ACTION =
        "Invalid action '{0}' for current game state";
    constexpr std::string_view RATE_LIMITED =
        "Rate limit exceeded. Please wait before sending more messages";
    constexpr std::string_view INVALID_SEQUENCE =
        "Invalid sequence number. Expected: {0}, Got: {1}";
    constexpr std::string_view AUTH_REQUIRED =
        "Authentication required. Please send handshake message first";
    constexpr std::string_view SESSION_EXPIRED =
        "Session expired. Please reconnect to establish new session";
    constexpr std::string_view MALFORMED_JSON =
        "Invalid JSON format: {0}";
    constexpr std::string_view INVALID_AMOUNT =
        "Invalid amount: {0}. Must be between {1} and {2}";
}

/// Format a template by replacing {N} placeholders with the provided arguments.
/// Returns the formatted string, or the template unchanged on error.
template <typename... Args>
[[nodiscard]] std::string format_error(std::string_view tmpl, const Args&... args) {
    // Simple positional formatting: replaces {0}, {1}, {2} etc.
    // For now, a basic implementation that handles up to the number of args.
    std::string result(tmpl);
    int idx = 0;
    ([&] {
        std::string placeholder = "{" + std::to_string(idx++) + "}";
        auto pos = result.find(placeholder);
        while (pos != std::string::npos) {
            result.replace(pos, placeholder.size(), args);
            pos = result.find(placeholder, pos + sizeof(args) - 1);
        }
    }(), ...);
    return result;
}

} // namespace server
} // namespace cppsim
