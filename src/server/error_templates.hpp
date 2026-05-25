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
/// Args must be convertible to std::string_view.
template <typename... Args>
[[nodiscard]] std::string format_error(std::string_view tmpl, const Args&... args) {
    // Simple positional formatting: replaces {0}, {1}, {2} etc.
    std::string result;
    result.reserve(tmpl.size());

    // Build a lookup table: arg_strings[i] is the string for placeholder {i}
    const std::string_view arg_strings[] = {std::string_view(args)...};
    constexpr size_t num_args = sizeof...(Args);

    size_t pos = 0;
    while (pos < tmpl.size()) {
        auto open = tmpl.find('{', pos);
        if (open == std::string_view::npos) {
            result.append(tmpl.substr(pos));
            break;
        }
        // Copy text before the placeholder
        result.append(tmpl.substr(pos, open - pos));

        auto close = tmpl.find('}', open + 1);
        if (close == std::string_view::npos) {
            // Malformed template — no closing brace. Append rest as-is.
            result.append(tmpl.substr(open));
            break;
        }

        // Extract the index between { and }
        size_t idx_len = close - open - 1;
        if (idx_len == 0 || idx_len > 1) {
            // Single-digit indices only (0-9). For multi-digit or empty, skip.
            result.append(tmpl.substr(open, close - open + 1));
            pos = close + 1;
            continue;
        }

        char digit = tmpl[open + 1];
        if (digit < '0' || digit > '9') {
            result.append(tmpl.substr(open, close - open + 1));
            pos = close + 1;
            continue;
        }

        size_t idx = static_cast<size_t>(digit - '0');
        if (idx < num_args) {
            result.append(arg_strings[idx]);
        } else {
            // Index out of range — leave placeholder intact
            result.append(tmpl.substr(open, close - open + 1));
        }
        pos = close + 1;
    }

    return result;
}

} // namespace server
} // namespace cppsim
