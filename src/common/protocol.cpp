#include "protocol.hpp"

#include <cmath>
#include <cstdio>
#include <limits>
#include <mutex>
#include <unordered_set>

namespace cppsim {
namespace protocol {

namespace {

constexpr size_t MAX_MESSAGE_TYPE_LENGTH = 32;

std::function<void(std::string_view)> error_logger = [](std::string_view msg) {
    constexpr size_t max_safe_size = static_cast<size_t>(std::numeric_limits<int>::max());
    auto safe_size = static_cast<int>(std::min(msg.size(), max_safe_size));
    std::fprintf(stderr, "%.*s\n", safe_size, msg.data());
};
std::mutex logger_mutex;

struct string_view_hash {
  std::size_t operator()(std::string_view sv) const noexcept {
    return std::hash<std::string_view>{}(sv);
  }
};

using string_view_set = std::unordered_set<std::string_view, string_view_hash>;

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
  std::function<void(std::string_view)> logger_copy;
  {
    std::lock_guard<std::mutex> lock(logger_mutex);
    logger_copy = error_logger;
  }
  if (logger_copy) {
    logger_copy(msg);
  }
}

}  // namespace

std::optional<std::string> extract_message_type(std::string_view json_str) noexcept {
  try {
    auto j = nlohmann::json::parse(json_str);
    if (j.contains("message_type") && j["message_type"].is_string()) {
      auto msg_type = j["message_type"].get<std::string>();
      if (msg_type.size() > MAX_MESSAGE_TYPE_LENGTH) {
        log_protocol_error("[Protocol] message_type exceeds maximum length");
        return std::nullopt;
      }
      return msg_type;
    }
    log_protocol_error("[Protocol] Missing or invalid 'message_type' field in message");
    return std::nullopt;
  } catch (const std::exception& e) {
    log_protocol_error(std::string("[Protocol] JSON parse error in extract_message_type: ") + e.what());
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
  env.payload = payload;

  nlohmann::json j;
  to_json(j, env);
  return j.dump();
}

template <typename T>
std::optional<T> parse_message(std::string_view json_str, std::string_view expected_type,
                                std::string_view message_name) {
  try {
    auto j = nlohmann::json::parse(json_str);
    auto envelope = j.get<message_envelope>();
    if (envelope.message_type != expected_type) {
      return std::nullopt;
    }
    T msg;
    from_json(envelope.payload, msg);
    return msg;
  } catch (const std::exception& e) {
    log_protocol_error(std::string("[Protocol] ") + std::string(message_name) + " Parse Error: " + e.what());
    return std::nullopt;
  }
}
}

void set_error_logger(std::function<void(std::string_view)> logger) {
  std::lock_guard<std::mutex> lock(logger_mutex);
  error_logger = std::move(logger);
}

std::optional<handshake_message> parse_handshake(std::string_view json_str) {
  try {
    auto j = nlohmann::json::parse(json_str);
    auto envelope = j.get<message_envelope>();
    
    if (envelope.message_type != message_types::HANDSHAKE) {
      return std::nullopt;
    }
    
    handshake_message msg;
    from_json(envelope.payload, msg);
    
    if (msg.protocol_version != envelope.protocol_version) {
      log_protocol_error("[Protocol] Handshake version mismatch between payload and envelope");
      return std::nullopt;
    }
    
    return msg;
  } catch (const std::exception& e) {
    log_protocol_error(std::string("[Protocol] Handshake Parse Error: ") + e.what());
    return std::nullopt;
  }
}

std::optional<action_message> parse_action(std::string_view json_str) {
  auto result = parse_message<action_message>(json_str, message_types::ACTION, "Action");
  if (!result) {
    return result;
  }

  const auto& msg = *result;

  const auto& valid_types = get_valid_action_types();
  if (valid_types.find(msg.action_type) == valid_types.end()) {
    log_protocol_error("[Protocol] Invalid action_type: " + msg.action_type);
    return std::nullopt;
  }

  if (msg.amount && (*msg.amount <= 0 || !std::isfinite(*msg.amount) || *msg.amount > MAX_AMOUNT)) {
    log_protocol_error("[Protocol] Invalid amount in action: must be positive, finite, and within bounds");
    return std::nullopt;
  }

  const auto& amount_required = get_amount_required_types();
  if (amount_required.find(msg.action_type) != amount_required.end() && !msg.amount) {
    log_protocol_error("[Protocol] " + msg.action_type + " action requires amount field");
    return std::nullopt;
  }

  const auto& amount_forbidden = get_amount_forbidden_types();
  if (amount_forbidden.find(msg.action_type) != amount_forbidden.end() && msg.amount) {
    log_protocol_error("[Protocol] " + msg.action_type + " action should not have amount field");
    return std::nullopt;
  }

  return result;
}

std::optional<reload_request_message> parse_reload_request(std::string_view json_str) {
  auto result = parse_message<reload_request_message>(json_str, message_types::RELOAD_REQUEST,
                                                "Reload Request");
  if (!result) {
    return result;
  }

  const auto& msg = *result;

  if (msg.requested_amount < 0 || !std::isfinite(msg.requested_amount) || msg.requested_amount > MAX_RELOAD_AMOUNT) {
    log_protocol_error("[Protocol] Invalid reload amount: must be non-negative, finite, and within bounds");
    return std::nullopt;
  }

  return result;
}

std::optional<disconnect_message> parse_disconnect(std::string_view json_str) {
  return parse_message<disconnect_message>(json_str, message_types::DISCONNECT, "Disconnect");
}

// Serialization functions
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
