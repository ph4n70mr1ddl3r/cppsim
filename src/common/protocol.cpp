#include "protocol.hpp"
#include <cmath>
#include <iostream>

namespace cppsim {
namespace protocol {

namespace {
void log_protocol_error(const std::string& msg) {
  std::cerr << msg << std::endl;
}

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
}

template <typename T, typename MessageType>
std::optional<T> parse_message(const std::string& json_str, MessageType expected_type,
                               const char* message_name) {
  try {
    auto j = nlohmann::json::parse(json_str);
    auto envelope = j.get<message_envelope>();
    if (envelope.message_type != expected_type) {
      return std::nullopt;
    }
    T msg;
    from_json(envelope.payload, msg);
    return msg;
  } catch (const nlohmann::json::exception& e) {
    log_protocol_error(std::string("[Protocol] ") + message_name + " JSON Parse Error: " + e.what());
    return std::nullopt;
  } catch (const std::exception& e) {
    log_protocol_error(std::string("[Protocol] ") + message_name + " Parse Error: " + e.what());
    return std::nullopt;
  }
}

std::optional<handshake_message> parse_handshake(const std::string& json_str) {
  try {
    auto j = nlohmann::json::parse(json_str);
    auto envelope = j.get<message_envelope>();
    if (envelope.message_type != message_types::HANDSHAKE) {
      return std::nullopt;
    }
    handshake_message msg;
    try {
      from_json(envelope.payload, msg);
    } catch (const std::exception& e) {
      log_protocol_error(std::string("[Protocol] Handshake Payload Error: ") + e.what());
      return std::nullopt;
    }

    if (msg.protocol_version != envelope.protocol_version) {
      return std::nullopt;
    }

    return msg;
  } catch (const nlohmann::json::exception& e) {
    log_protocol_error(std::string("[Protocol] Handshake JSON Parse Error: ") + e.what());
    return std::nullopt;
  } catch (const std::exception& e) {
    log_protocol_error(std::string("[Protocol] Handshake Parse Error: ") + e.what());
    return std::nullopt;
  }
}

std::optional<action_message> parse_action(const std::string& json_str) {
  auto result = parse_message<action_message>(json_str, message_types::ACTION, "Action");
  if (!result) {
    return result;
  }

  const auto& msg = *result;

  // Validate action_type is one of the allowed values
  if (msg.action_type != action_types::FOLD && 
      msg.action_type != action_types::CHECK &&
      msg.action_type != action_types::CALL &&
      msg.action_type != action_types::RAISE &&
      msg.action_type != action_types::ALL_IN) {
    log_protocol_error("[Protocol] Invalid action_type: " + msg.action_type);
    return std::nullopt;
  }

  if (msg.amount && (*msg.amount <= 0 || !std::isfinite(*msg.amount))) {
    log_protocol_error("[Protocol] Invalid amount in action: must be positive and finite");
    return std::nullopt;
  }

  if ((msg.action_type == action_types::RAISE || msg.action_type == action_types::ALL_IN) && !msg.amount) {
    log_protocol_error("[Protocol] " + msg.action_type + " action requires amount field");
    return std::nullopt;
  }

  if ((msg.action_type == action_types::FOLD || msg.action_type == action_types::CHECK ||
       msg.action_type == action_types::CALL) && msg.amount) {
    log_protocol_error("[Protocol] " + msg.action_type + " action should not have amount field");
    return std::nullopt;
  }

  return result;
}

std::optional<reload_request_message> parse_reload_request(const std::string& json_str) {
  auto result = parse_message<reload_request_message>(json_str, message_types::RELOAD_REQUEST,
                                                "Reload Request");
  if (!result) {
    return result;
  }

  const auto& msg = *result;

  if (msg.requested_amount < 0 || !std::isfinite(msg.requested_amount)) {
    log_protocol_error("[Protocol] Invalid reload amount: must be non-negative and finite");
    return std::nullopt;
  }

  return result;
}

std::optional<disconnect_message> parse_disconnect(const std::string& json_str) {
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