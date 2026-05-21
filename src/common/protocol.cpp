#include "protocol.hpp"
#include "string_utils.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <unordered_set>

namespace cppsim {
namespace protocol {

namespace {

constexpr size_t MAX_MESSAGE_TYPE_LENGTH = 32;
constexpr size_t MAX_CLIENT_NAME_LENGTH = 128;
constexpr size_t MAX_DISCONNECT_REASON_LENGTH = 256;

// trunc_field is provided by common/string_utils.hpp in the cppsim namespace.
// It is accessible via unqualified lookup from cppsim::protocol (enclosing namespace).

constexpr bool is_printable_ascii(char c) noexcept { return static_cast<unsigned char>(c) >= 0x20 && static_cast<unsigned char>(c) < 0x7f; }

constexpr bool is_valid_session_id_char(char c) noexcept {
  return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

bool validate_session_id_format(const std::string& sid) noexcept {
  // Minimum: "sess_" + at least 1 hex char
  if (sid.size() < 6 || sid.size() > MAX_SESSION_ID_LENGTH) return false;
  if (sid[0] != 's' || sid[1] != 'e' || sid[2] != 's' || sid[3] != 's' || sid[4] != '_') return false;
  for (size_t i = 5; i < sid.size(); ++i) {
    if (!is_valid_session_id_char(sid[i])) return false;
  }
  return true;
}

std::mutex error_logger_mutex;
std::function<void(std::string_view)> error_logger =
    [](std::string_view msg) {
      constexpr size_t max_safe_size = static_cast<size_t>(std::numeric_limits<int>::max());
      auto safe_size = static_cast<int>(std::min(msg.size(), max_safe_size));
      std::fprintf(stderr, "%.*s\n", safe_size, msg.data());
    };

using string_view_set = std::unordered_set<std::string_view>;

const string_view_set& get_valid_action_types() noexcept {
  static const string_view_set valid_types = {
    action_types::FOLD,
    action_types::CHECK,
    action_types::CALL,
    action_types::RAISE,
    action_types::ALL_IN
  };
  return valid_types;
}

const string_view_set& get_amount_required_types() noexcept {
  static const string_view_set amount_required = {
    action_types::RAISE,
    action_types::ALL_IN
  };
  return amount_required;
}

const string_view_set& get_amount_forbidden_types() noexcept {
  static const string_view_set amount_forbidden = {
    action_types::FOLD,
    action_types::CHECK,
    action_types::CALL
  };
  return amount_forbidden;
}

void log_protocol_error(std::string_view msg) noexcept {
  try {
    std::function<void(std::string_view)> logger_copy;
    {
      std::lock_guard<std::mutex> lock(error_logger_mutex);
      logger_copy = error_logger;
    }
    // Call the logger while holding the copy to ensure thread safety
    if (logger_copy) {
      logger_copy(msg);
    }
  } catch (...) {
    // Last resort: fprintf to stderr is atomic on POSIX
    std::fprintf(stderr, "[Protocol Error] %.*s\n",
                 static_cast<int>(std::min(msg.size(), static_cast<size_t>(1024))),
                 msg.data());
  }
}

// Validate session ID format for message types that require it.
// Returns true if valid, logs an error and returns false otherwise.
// String construction is wrapped in try/catch because this function is
// noexcept — an allocation failure in string concatenation would otherwise
// call std::terminate.
bool validate_session_id_field(const std::string& session_id, const char* message_name) noexcept {
  if (session_id.empty() || !validate_session_id_format(session_id)) {
    try {
      log_protocol_error("[Protocol] Invalid session_id in " + std::string(message_name) + " message");
    } catch (...) {
      // Allocation failure in string construction — nothing useful to do.
      // The caller will return nullopt regardless.
    }
    return false;
  }
  return true;
}

}  // namespace

std::optional<std::string> extract_message_type(std::string_view json_str) noexcept {
  auto result = extract_message_type_and_json(json_str);
  if (result) {
    return result->message_type;
  }
  return std::nullopt;
}

std::optional<parsed_message_header> extract_message_type_and_json(std::string_view json_str) noexcept {
  try {
    auto j = nlohmann::json::parse(json_str);
    if (j.contains("message_type") && j["message_type"].is_string()) {
      auto msg_type = j["message_type"].get<std::string>();
      if (msg_type.size() > MAX_MESSAGE_TYPE_LENGTH) {
        log_protocol_error("[Protocol] message_type exceeds maximum length");
        return std::nullopt;
      }
      return parsed_message_header{std::move(msg_type), std::move(j)};
    }
    log_protocol_error("[Protocol] Missing or invalid 'message_type' field in message");
    return std::nullopt;
  } catch (const std::exception& e) {
    try {
      log_protocol_error(std::string("[Protocol] JSON parse error in extract_message_type: ") + e.what());
    } catch (...) {
    }
    return std::nullopt;
  }
}

namespace {

template <typename MessageType>
std::string serialize_message(const MessageType& msg, const char* message_type) {
  message_envelope env;
  env.message_type = message_type;
  env.protocol_version = PROTOCOL_VERSION;
  nlohmann::json payload;
  to_json(payload, msg);
  env.payload = std::move(payload);

  nlohmann::json j;
  to_json(j, env);
  return j.dump();
}

// Parse from pre-parsed envelope JSON (avoids double JSON parse)
template <typename T>
std::optional<T> parse_from_envelope(const nlohmann::json& envelope_json, std::string_view expected_type,
                                      std::string_view message_name) {
  try {
    auto envelope = envelope_json.get<message_envelope>();
    if (envelope.message_type != expected_type) {
      return std::nullopt;
    }
    if (envelope.protocol_version != PROTOCOL_VERSION) {
      log_protocol_error("[Protocol] " + std::string(message_name) + " version mismatch: expected " +
                         PROTOCOL_VERSION + ", got " + envelope.protocol_version);
      return std::nullopt;
    }
    T msg;
    from_json(envelope.payload, msg);
    return msg;
  } catch (const std::exception& e) {
    try {
      log_protocol_error("[Protocol] " + std::string(message_name) + " Parse Error: " + e.what());
    } catch (...) {
    }
    return std::nullopt;
  }
}

template <typename T>
std::optional<T> parse_message(std::string_view json_str, std::string_view expected_type,
                                 std::string_view message_name) {
  try {
    auto j = nlohmann::json::parse(json_str);
    return parse_from_envelope<T>(j, expected_type, message_name);
  } catch (const std::exception& e) {
    try {
      log_protocol_error("[Protocol] " + std::string(message_name) + " Parse Error: " + e.what());
    } catch (...) {
    }
    return std::nullopt;
  }
}

}  // namespace

void set_error_logger(std::function<void(std::string_view)> logger) {
  std::lock_guard<std::mutex> lock(error_logger_mutex);
  error_logger = std::move(logger);
}

// NOTE: Unlike parse_message<>, parse_handshake does NOT validate the envelope
// protocol_version.  The caller (handle_handshake_message) performs version
// checking so it can send a version-specific error response before closing.
// This is intentional — see HandshakeEnvelopeVersionMismatch test.
std::optional<handshake_message> parse_handshake(std::string_view json_str) {
  try {
    auto j = nlohmann::json::parse(json_str);
    auto envelope = j.get<message_envelope>();
    
    if (envelope.message_type != message_types::HANDSHAKE) {
      return std::nullopt;
    }
    
    handshake_message msg;
    from_json(envelope.payload, msg);
    
    if (msg.protocol_version.empty()) {
      log_protocol_error("[Protocol] Empty protocol_version in HANDSHAKE message");
      return std::nullopt;
    }

    if (msg.client_name && msg.client_name->size() > MAX_CLIENT_NAME_LENGTH) {
      log_protocol_error("[Protocol] Client name exceeds maximum length");
      return std::nullopt;
    }

    if (msg.client_name) {
      const auto& name = *msg.client_name;
      bool all_blank = true;
      for (char c : name) {
        if (!is_printable_ascii(c)) {
          log_protocol_error("[Protocol] Client name contains non-printable characters");
          return std::nullopt;
        }
        if (c != ' ') {
          all_blank = false;
        }
      }
      if (all_blank) {
        log_protocol_error("[Protocol] Client name is blank/whitespace-only");
        return std::nullopt;
      }
    }
    
    if (msg.protocol_version != envelope.protocol_version) {
      log_protocol_error("[Protocol] Handshake version mismatch between payload and envelope");
      return std::nullopt;
    }
    
    return msg;
  } catch (const std::exception& e) {
    try {
      log_protocol_error(std::string("[Protocol] Handshake Parse Error: ") + e.what());
    } catch (...) {
    }
    return std::nullopt;
  }
}

// Validate action_message fields after deserialization.
// Takes by value: C++17 guaranteed copy elision makes parameter construction
// from a prvalue zero-cost, and std::move on the success path avoids string
// copies (pointer swaps vs. heap allocations).
std::optional<action_message> validate_action(std::optional<action_message> result) {
  if (!result) return std::nullopt;
  const auto& msg = *result;

  if (!validate_session_id_field(msg.session_id, "ACTION")) {
    return std::nullopt;
  }

  if (msg.sequence_number < 0) {
    log_protocol_error("[Protocol] Negative sequence_number in ACTION message");
    return std::nullopt;
  }

  if (msg.action_type.empty()) {
    log_protocol_error("[Protocol] Empty action_type in ACTION message");
    return std::nullopt;
  }

  const auto& valid_types = get_valid_action_types();
  if (valid_types.find(msg.action_type) == valid_types.end()) {
    log_protocol_error("[Protocol] Invalid action_type: " + trunc_field(msg.action_type));
    return std::nullopt;
  }

  if (msg.amount && (*msg.amount <= 0 || !std::isfinite(*msg.amount) || *msg.amount > MAX_AMOUNT)) {
    log_protocol_error("[Protocol] Invalid amount in action: must be positive, finite, and within bounds");
    return std::nullopt;
  }

  const auto& amount_required = get_amount_required_types();
  if (amount_required.find(msg.action_type) != amount_required.end() && !msg.amount) {
    log_protocol_error("[Protocol] " + trunc_field(msg.action_type) + " action requires amount field");
    return std::nullopt;
  }

  const auto& amount_forbidden = get_amount_forbidden_types();
  if (amount_forbidden.find(msg.action_type) != amount_forbidden.end() && msg.amount) {
    log_protocol_error("[Protocol] " + trunc_field(msg.action_type) + " action should not have amount field");
    return std::nullopt;
  }

  return result;
}

std::optional<action_message> parse_action(std::string_view json_str) {
  return validate_action(parse_message<action_message>(json_str, message_types::ACTION, "Action"));
}

std::optional<action_message> parse_action_from_envelope(const nlohmann::json& envelope_json) {
  return validate_action(parse_from_envelope<action_message>(envelope_json, message_types::ACTION, "Action"));
}

// Validate reload_request_message fields after deserialization.
// Takes by value for zero-copy success path (C++17 guaranteed copy elision).
std::optional<reload_request_message> validate_reload(std::optional<reload_request_message> result) {
  if (!result) return std::nullopt;
  const auto& msg = *result;

  if (!validate_session_id_field(msg.session_id, "RELOAD_REQUEST")) {
    return std::nullopt;
  }

  if (msg.requested_amount <= 0 || !std::isfinite(msg.requested_amount) || msg.requested_amount > MAX_AMOUNT) {
    log_protocol_error("[Protocol] Invalid reload amount: must be positive, finite, and within bounds");
    return std::nullopt;
  }

  return result;
}

std::optional<reload_request_message> parse_reload_request(std::string_view json_str) {
  return validate_reload(parse_message<reload_request_message>(json_str, message_types::RELOAD_REQUEST,
                                                "Reload Request"));
}

std::optional<reload_request_message> parse_reload_from_envelope(const nlohmann::json& envelope_json) {
  return validate_reload(parse_from_envelope<reload_request_message>(envelope_json, message_types::RELOAD_REQUEST,
                                                "Reload Request"));
}

// Validate disconnect_message fields after deserialization.
// Takes by value for zero-copy success path (C++17 guaranteed copy elision).
std::optional<disconnect_message> validate_disconnect(std::optional<disconnect_message> result) {
  if (!result) return std::nullopt;
  const auto& msg = *result;

  if (!validate_session_id_field(msg.session_id, "DISCONNECT")) {
    return std::nullopt;
  }

  if (msg.reason) {
    if (msg.reason->size() > MAX_DISCONNECT_REASON_LENGTH) {
      log_protocol_error("[Protocol] Disconnect reason exceeds maximum length");
      return std::nullopt;
    }
    for (char c : *msg.reason) {
      if (!is_printable_ascii(c)) {
        log_protocol_error("[Protocol] Disconnect reason contains non-printable characters");
        return std::nullopt;
      }
    }
  }

  return result;
}

std::optional<disconnect_message> parse_disconnect(std::string_view json_str) {
  return validate_disconnect(parse_message<disconnect_message>(json_str, message_types::DISCONNECT, "Disconnect"));
}

std::optional<disconnect_message> parse_disconnect_from_envelope(const nlohmann::json& envelope_json) {
  return validate_disconnect(parse_from_envelope<disconnect_message>(envelope_json, message_types::DISCONNECT, "Disconnect"));
}

std::string serialize_state_update(const state_update_message& msg) {
  return serialize_message(msg, message_types::STATE_UPDATE);
}

std::string serialize_error(const error_message& msg) {
  return serialize_message(msg, message_types::ERROR);
}

std::string serialize_handshake_response(const handshake_response& msg) {
  return serialize_message(msg, message_types::HANDSHAKE_RESPONSE);
}

std::string serialize_reload_response(const reload_response_message& msg) {
  return serialize_message(msg, message_types::RELOAD_RESPONSE);
}

}  // namespace protocol
}  // namespace cppsim
