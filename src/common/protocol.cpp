#include "protocol.hpp"
#include <cmath>
#include <cstdio>
#include <functional>
#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_set>

namespace cppsim {
namespace protocol {

namespace {
std::function<void(std::string_view)> error_logger = [](std::string_view msg) {
    std::fprintf(stderr, "%.*s\n", static_cast<int>(msg.size()), msg.data());
};
std::mutex logger_mutex;

const std::unordered_set<std::string>& get_valid_action_types() {
  static const std::unordered_set<std::string> valid_types = {
    std::string(action_types::FOLD),
    std::string(action_types::CHECK),
    std::string(action_types::CALL),
    std::string(action_types::RAISE),
    std::string(action_types::ALL_IN)
  };
  return valid_types;
}

const std::unordered_set<std::string>& get_amount_required_types() {
  static const std::unordered_set<std::string> amount_required = {
    std::string(action_types::RAISE),
    std::string(action_types::ALL_IN)
  };
  return amount_required;
}

const std::unordered_set<std::string>& get_amount_forbidden_types() {
  static const std::unordered_set<std::string> amount_forbidden = {
    std::string(action_types::FOLD),
    std::string(action_types::CHECK),
    std::string(action_types::CALL)
  };
  return amount_forbidden;
}

void log_protocol_error(std::string_view msg) {
  std::lock_guard<std::mutex> lock(logger_mutex);
  error_logger(msg);
}

}  // namespace

std::optional<std::string> extract_message_type(std::string_view json_str) noexcept {
  try {
    auto j = nlohmann::json::parse(json_str);
    if (j.contains("message_type") && j["message_type"].is_string()) {
      return j["message_type"].get<std::string>();
    }
    return std::nullopt;
  } catch (...) {
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
  } catch (const std::exception& e) {
    log_protocol_error(std::string("[Protocol] ") + message_name + " Parse Error: " + e.what());
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

  if (msg.amount && (*msg.amount <= 0 || !std::isfinite(*msg.amount))) {
    log_protocol_error("[Protocol] Invalid amount in action: must be positive and finite");
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

  if (msg.requested_amount < 0 || !std::isfinite(msg.requested_amount)) {
    log_protocol_error("[Protocol] Invalid reload amount: must be non-negative and finite");
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
