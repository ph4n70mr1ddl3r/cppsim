#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace cppsim {
namespace protocol {

/**
 * @brief Enhanced validation utilities for protocol messages
 * 
 * Provides comprehensive validation functions for all protocol message types
 * with detailed error reporting and input sanitization.
 */
namespace validation {
    
    /**
     * @brief Validates message types against allowed values
     * @param message_type The message type to validate
     * @param allowed_types List of allowed message types
     * @return true if valid, false otherwise
     */
    [[nodiscard]] bool is_valid_message_type(const std::string& message_type,
                                            const std::vector<std::string>& allowed_types) noexcept;
    
    /**
     * @brief Validates protocol version format
     * @param version The version string to validate
     * @return true if valid, false otherwise
     */
    [[nodiscard]] bool is_valid_protocol_version(const std::string& version) noexcept;
    
    /**
     * @brief Validates session ID format
     * @param session_id The session ID to validate
     * @return true if valid, false otherwise
     */
    [[nodiscard]] bool is_valid_session_id(const std::string& session_id) noexcept;
    
    /**
     * @brief validates action types
     * @param action_type The action type to validate
     * @return true if valid, false otherwise
     */
    [[nodiscard]] bool is_valid_action_type(const std::string& action_type) noexcept;
    
    /**
     * @brief Validates monetary amounts
     * @param amount The amount to validate
     * @param min_amount Minimum allowed amount (optional)
     * @param max_amount Maximum allowed amount (optional)
     * @return true if valid, false otherwise
     */
    [[nodiscard]] bool is_valid_amount(int64_t amount,
                                      std::optional<int64_t> min_amount = std::nullopt,
                                      std::optional<int64_t> max_amount = std::nullopt) noexcept;
    
    /**
     * @brief Validates game phase strings
     * @param game_phase The game phase to validate
     * @return true if valid, false otherwise
     */
    [[nodiscard]] bool is_valid_game_phase(const std::string& game_phase) noexcept;
    
    /**
     * @brief Validates seat numbers
     * @param seat The seat number to validate
     * @param max_seats Maximum allowed seats
     * @return true if valid, false otherwise
     */
    [[nodiscard]] bool is_valid_seat_number(int seat, int max_seats = 10) noexcept;
    
    /**
     * @brief Sanitizes string input for safe logging and transmission
     * @param input The input string to sanitize
     * @return Sanitized string with control characters removed
     */
    [[nodiscard]] std::string sanitize_input(const std::string& input) noexcept;
    
    /**
     * @brief Validates JSON structure for message envelopes
     * @param json_str JSON string to validate
     * @param required_fields List of required top-level fields
     * @return true if valid JSON with required fields, false otherwise
     */
    [[nodiscard]] bool validate_message_envelope(const std::string& json_str,
                                                const std::vector<std::string>& required_fields) noexcept;
    
    /**
     * @brief Validates sequence number gap
     * @param current Current sequence number
     * @param previous Previous sequence number
     * @param max_gap Maximum allowed gap
     * @return true if valid gap, false otherwise
     */
    [[nodiscard]] bool validate_sequence_gap(int64_t current, int64_t previous, int64_t max_gap = 10000) noexcept;
    
} // namespace validation

} // namespace protocol
} // namespace cppsim

namespace cppsim {
namespace protocol {

void set_error_logger(std::function<void(std::string_view)> logger);

// Protocol version constant
// Version format: v{major}.{minor}
// Major version changes are incompatible (require new handshake)
// Minor version changes are backward compatible
constexpr const char* PROTOCOL_VERSION = "v1.0";

// Validation constants
constexpr int64_t MAX_AMOUNT = 1000000000000000LL;  // Maximum valid amount (in cents)
// Maximum length for session IDs. Must match server config.
// Generated IDs are "sess_" + 32 hex chars = 37 bytes; the limit allows future formats.
constexpr size_t MAX_SESSION_ID_LENGTH = 128;

// Error Codes
namespace error_codes {
constexpr const char* INCOMPATIBLE_VERSION = "INCOMPATIBLE_VERSION";
constexpr const char* PROTOCOL_ERROR = "PROTOCOL_ERROR";
constexpr const char* MALFORMED_HANDSHAKE = "MALFORMED_HANDSHAKE";
constexpr const char* MALFORMED_MESSAGE = "MALFORMED_MESSAGE";
constexpr const char* SESSION_CLOSED = "SESSION_CLOSED";
}

// Message Types
namespace message_types {
constexpr const char* HANDSHAKE = "HANDSHAKE";
constexpr const char* HANDSHAKE_RESPONSE = "HANDSHAKE_RESPONSE";
constexpr const char* ACTION = "ACTION";
constexpr const char* STATE_UPDATE = "STATE_UPDATE";
constexpr const char* ERROR = "ERROR";
constexpr const char* RELOAD_REQUEST = "RELOAD_REQUEST";
constexpr const char* RELOAD_RESPONSE = "RELOAD_RESPONSE";
constexpr const char* DISCONNECT = "DISCONNECT";
}

// Action Types
namespace action_types {
constexpr const char* FOLD = "FOLD";
constexpr const char* CHECK = "CHECK";
constexpr const char* CALL = "CALL";
constexpr const char* RAISE = "RAISE";
constexpr const char* ALL_IN = "ALL_IN";
}

// Player stack information for state updates
struct player_stack {
  int seat;
  int64_t stack;  // Amount in cents
};

// HANDSHAKE message - Client initiates connection
struct handshake_message {
  std::string protocol_version;
  std::optional<std::string> client_name;
};

// HANDSHAKE response - Server assigns session
struct handshake_response {
  std::string session_id;
  int seat_number;
  int64_t starting_stack;  // Amount in cents
};

// ACTION message - Client sends poker action
struct action_message {
  std::string session_id;
  std::string action_type;  // "FOLD", "CALL", "RAISE", "CHECK", "ALL_IN"
  std::optional<int64_t> amount;  // Amount in cents, if applicable
  int64_t sequence_number;
};

// STATE_UPDATE message - Server broadcasts game state
struct state_update_message {
  std::string game_phase;  // "WAITING", "PREFLOP", "FLOP", "TURN", "RIVER",
                           // "SHOWDOWN", "HAND_COMPLETE"
  int64_t pot_size;       // Amount in cents
  int64_t current_bet;    // Amount in cents
  std::vector<player_stack> player_stacks;
  std::optional<std::vector<std::string>> community_cards;
  std::optional<std::vector<std::string>> hole_cards;
  std::vector<std::string> valid_actions;
  std::optional<int> acting_seat;
};

// ERROR message - Server reports error
struct error_message {
  std::string error_code;  // "INVALID_ACTION", "OUT_OF_TURN",
                           // "INSUFFICIENT_STACK", "MALFORMED_MESSAGE", etc.
  std::string message;     // Human-readable description
  std::optional<std::string> session_id;
};

// RELOAD_REQUEST message - Client requests chip reload
struct reload_request_message {
  std::string session_id;
  int64_t requested_amount;  // Amount in cents
};

// RELOAD_RESPONSE message - Server responds to reload request
struct reload_response_message {
  bool granted;
  int64_t new_stack;  // Amount in cents
};

// DISCONNECT message - Graceful disconnection
struct disconnect_message {
  std::string session_id;
  std::optional<std::string> reason;
};

// Message envelope for routing
struct message_envelope {
  std::string message_type;  // "HANDSHAKE", "ACTION", "STATE_UPDATE", etc.
  std::string protocol_version;
  nlohmann::json payload;
};

// Parsing functions - return std::optional for safe error handling
// These functions log errors internally and return std::nullopt on parse failure
[[nodiscard]] std::optional<handshake_message> parse_handshake(std::string_view json_str);
[[nodiscard]] std::optional<action_message> parse_action(std::string_view json_str);
[[nodiscard]] std::optional<reload_request_message> parse_reload_request(std::string_view json_str);
[[nodiscard]] std::optional<disconnect_message> parse_disconnect(std::string_view json_str);

/// Parse from pre-parsed envelope JSON (avoids double-parsing).
/// The envelope_json must contain "payload" and "protocol_version" fields.
[[nodiscard]] std::optional<action_message> parse_action_from_envelope(const nlohmann::json& envelope_json);
[[nodiscard]] std::optional<reload_request_message> parse_reload_from_envelope(const nlohmann::json& envelope_json);
[[nodiscard]] std::optional<disconnect_message> parse_disconnect_from_envelope(const nlohmann::json& envelope_json);

[[nodiscard]] std::optional<std::string> extract_message_type(std::string_view json_str) noexcept;

/// Extract message type and pre-parsed JSON to avoid double-parsing.
/// Returns nullopt on parse failure.
struct parsed_message_header {
  std::string message_type;
  nlohmann::json envelope_json;
};
[[nodiscard]] std::optional<parsed_message_header> extract_message_type_and_json(std::string_view json_str) noexcept;

[[nodiscard]] std::string serialize_state_update(const state_update_message& msg);
[[nodiscard]] std::string serialize_error(const error_message& msg);
[[nodiscard]] std::string serialize_handshake_response(const handshake_response& msg);
[[nodiscard]] std::string serialize_reload_response(const reload_response_message& msg);

// nlohmann/json serialization functions
// Manual to_json/from_json for proper std::optional support
// Placed inside namespace for proper ADL (Argument Dependent Lookup)

inline void to_json(nlohmann::json& j, const player_stack& p) {
  j = nlohmann::json{{"seat", p.seat}, {"stack", p.stack}};
}

inline void from_json(const nlohmann::json& j, player_stack& p) {
  j.at("seat").get_to(p.seat);
  j.at("stack").get_to(p.stack);
}

inline void to_json(nlohmann::json& j, const handshake_message& m) {
  j = nlohmann::json{{"protocol_version", m.protocol_version}};
  if (m.client_name) j["client_name"] = *m.client_name;
}

inline void from_json(const nlohmann::json& j, handshake_message& m) {
  j.at("protocol_version").get_to(m.protocol_version);
  if (j.contains("client_name") && !j["client_name"].is_null()) {
    m.client_name = j["client_name"].template get<std::string>();
  }
}

inline void to_json(nlohmann::json& j, const handshake_response& m) {
  j = nlohmann::json{{"session_id", m.session_id},
                     {"seat_number", m.seat_number},
                     {"starting_stack", m.starting_stack}};
}

inline void from_json(const nlohmann::json& j, handshake_response& m) {
  j.at("session_id").get_to(m.session_id);
  j.at("seat_number").get_to(m.seat_number);
  j.at("starting_stack").get_to(m.starting_stack);
}

inline void to_json(nlohmann::json& j, const action_message& m) {
  j = nlohmann::json{{"session_id", m.session_id},
                     {"action_type", m.action_type},
                     {"sequence_number", m.sequence_number}};
  if (m.amount) j["amount"] = *m.amount;
}

inline void from_json(const nlohmann::json& j, action_message& m) {
  j.at("session_id").get_to(m.session_id);
  j.at("action_type").get_to(m.action_type);
  j.at("sequence_number").get_to(m.sequence_number);
  if (j.contains("amount") && !j["amount"].is_null()) {
    m.amount = j["amount"].get<int64_t>();
  }
}

inline void to_json(nlohmann::json& j, const state_update_message& m) {
  j = nlohmann::json{{"game_phase", m.game_phase},
                     {"pot_size", m.pot_size},
                     {"current_bet", m.current_bet},
                     {"player_stacks", m.player_stacks},
                     {"valid_actions", m.valid_actions}};
  if (m.community_cards) j["community_cards"] = *m.community_cards;
  if (m.hole_cards) j["hole_cards"] = *m.hole_cards;
  if (m.acting_seat) j["acting_seat"] = *m.acting_seat;
}

inline void from_json(const nlohmann::json& j, state_update_message& m) {
  j.at("game_phase").get_to(m.game_phase);
  j.at("pot_size").get_to(m.pot_size);
  j.at("current_bet").get_to(m.current_bet);

  j.at("player_stacks").get_to(m.player_stacks);

  j.at("valid_actions").get_to(m.valid_actions);
  if (j.contains("community_cards") && !j["community_cards"].is_null()) {
    m.community_cards = j["community_cards"].get<std::vector<std::string>>();
  }
  if (j.contains("hole_cards") && !j["hole_cards"].is_null()) {
    m.hole_cards = j["hole_cards"].get<std::vector<std::string>>();
  }
  if (j.contains("acting_seat") && !j["acting_seat"].is_null()) {
    m.acting_seat = j["acting_seat"].get<int>();
  }
}

inline void to_json(nlohmann::json& j, const error_message& m) {
  j = nlohmann::json{{"error_code", m.error_code}, {"message", m.message}};
  if (m.session_id) j["session_id"] = *m.session_id;
}

inline void from_json(const nlohmann::json& j, error_message& m) {
  j.at("error_code").get_to(m.error_code);
  j.at("message").get_to(m.message);
  if (j.contains("session_id") && !j["session_id"].is_null()) {
    m.session_id = j["session_id"].get<std::string>();
  }
}

inline void to_json(nlohmann::json& j, const reload_request_message& m) {
  j = nlohmann::json{{"session_id", m.session_id}, {"requested_amount", m.requested_amount}};
}

inline void from_json(const nlohmann::json& j, reload_request_message& m) {
  j.at("session_id").get_to(m.session_id);
  j.at("requested_amount").get_to(m.requested_amount);
}

inline void to_json(nlohmann::json& j, const reload_response_message& m) {
  j = nlohmann::json{{"granted", m.granted}, {"new_stack", m.new_stack}};
}

inline void from_json(const nlohmann::json& j, reload_response_message& m) {
  j.at("granted").get_to(m.granted);
  j.at("new_stack").get_to(m.new_stack);
}

inline void to_json(nlohmann::json& j, const disconnect_message& m) {
  j = nlohmann::json{{"session_id", m.session_id}};
  if (m.reason) j["reason"] = *m.reason;
}

inline void from_json(const nlohmann::json& j, disconnect_message& m) {
  j.at("session_id").get_to(m.session_id);
  if (j.contains("reason") && !j["reason"].is_null()) {
    m.reason = j["reason"].get<std::string>();
  }
}

inline void to_json(nlohmann::json& j, const message_envelope& m) {
  j = nlohmann::json{
      {"message_type", m.message_type}, {"protocol_version", m.protocol_version}, {"payload", m.payload}};
}

inline void from_json(const nlohmann::json& j, message_envelope& m) {
  j.at("message_type").get_to(m.message_type);
  j.at("protocol_version").get_to(m.protocol_version);
  j.at("payload").get_to(m.payload);
}

}  // namespace protocol
}  // namespace cppsim
