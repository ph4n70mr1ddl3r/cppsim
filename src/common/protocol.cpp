#include "protocol.hpp"
#include <iostream>

namespace cppsim {
namespace protocol {

// Parsing functions
std::optional<handshake_message> parse_handshake(const std::string& json_str) {
  try {
    auto j = nlohmann::json::parse(json_str);
    auto envelope = j.get<message_envelope>();
    if (envelope.message_type != message_types::HANDSHAKE) {
      return std::nullopt;
    }
    // Extract payload from envelope
    handshake_message msg;
    try {
      from_json(envelope.payload, msg);
    } catch (const std::exception& e) {
       std::cerr << "[Protocol] Handshake Payload Error: " << e.what() << std::endl;
       return std::nullopt;
    }
    
    // Resolve Ambiguity: Envelope is the transport source of truth.
    // If payload has a different version, we treat it as a protocol error or simply fail to parse.
    // Overwriting silently hides defects.
    if (msg.protocol_version != envelope.protocol_version) {
        return std::nullopt;
    }
    
    return msg;
  } catch (const nlohmann::json::exception& e) {
    std::cerr << "[Protocol] Handshake JSON Parse Error: " << e.what() << std::endl;
    return std::nullopt;
  } catch (const std::exception& e) {
    std::cerr << "[Protocol] Handshake Parse Error: " << e.what() << std::endl;
    return std::nullopt;
  }
}

std::optional<action_message> parse_action(const std::string& json_str) {
  try {
    auto j = nlohmann::json::parse(json_str);
    auto envelope = j.get<message_envelope>();
    if (envelope.message_type != message_types::ACTION) {
      return std::nullopt;
    }
    action_message msg;
    from_json(envelope.payload, msg);
    return msg;
  } catch (const nlohmann::json::exception& e) {
    std::cerr << "[Protocol] Action JSON Parse Error: " << e.what() << std::endl;
    return std::nullopt;
  } catch (const std::exception& e) {
    std::cerr << "[Protocol] Action Parse Error: " << e.what() << std::endl;
    return std::nullopt;
  }
}

std::optional<reload_request_message> parse_reload_request(const std::string& json_str) {
  try {
    auto j = nlohmann::json::parse(json_str);
    auto envelope = j.get<message_envelope>();
    if (envelope.message_type != message_types::RELOAD_REQUEST) {
      return std::nullopt;
    }
    reload_request_message msg;
    from_json(envelope.payload, msg);
    return msg;
  } catch (const nlohmann::json::exception& e) {
    std::cerr << "[Protocol] Reload Request JSON Parse Error: " << e.what() << std::endl;
    return std::nullopt;
  } catch (const std::exception& e) {
    std::cerr << "[Protocol] Reload Request Parse Error: " << e.what() << std::endl;
    return std::nullopt;
  }
}

std::optional<disconnect_message> parse_disconnect(const std::string& json_str) {
  try {
    auto j = nlohmann::json::parse(json_str);
    auto envelope = j.get<message_envelope>();
    if (envelope.message_type != message_types::DISCONNECT) {
      return std::nullopt;
    }
    disconnect_message msg;
    from_json(envelope.payload, msg);
    return msg;
  } catch (const nlohmann::json::exception& e) {
    std::cerr << "[Protocol] Disconnect JSON Parse Error: " << e.what() << std::endl;
    return std::nullopt;
  } catch (const std::exception& e) {
    std::cerr << "[Protocol] Disconnect Parse Error: " << e.what() << std::endl;
    return std::nullopt;
  }
}

// Serialization functions
std::string serialize_state_update(const state_update_message& msg) {
  message_envelope env;
  env.message_type = message_types::STATE_UPDATE;
  env.protocol_version = PROTOCOL_VERSION;
  nlohmann::json payload;
  to_json(payload, msg);
  env.payload = payload;
  
  nlohmann::json j;
  to_json(j, env);
  return j.dump();
}

std::string serialize_error(const error_message& msg) {
  message_envelope env;
  env.message_type = message_types::ERROR;
  env.protocol_version = PROTOCOL_VERSION;
  nlohmann::json payload;
  to_json(payload, msg);
  env.payload = payload;
  
  nlohmann::json j;
  to_json(j, env);
  return j.dump();
}

std::string serialize_handshake_response(const handshake_response& msg) {
  message_envelope env;
  env.message_type = message_types::HANDSHAKE_RESPONSE;
  env.protocol_version = PROTOCOL_VERSION;
  nlohmann::json payload;
  to_json(payload, msg);
  env.payload = payload;
  
  nlohmann::json j;
  to_json(j, env);
  return j.dump();
}

std::string serialize_reload_response(const reload_response_message& msg) {
  message_envelope env;
  env.message_type = message_types::RELOAD_RESPONSE;
  env.protocol_version = PROTOCOL_VERSION;
  nlohmann::json payload;
  to_json(payload, msg);
  env.payload = payload;
  
  nlohmann::json j;
  to_json(j, env);
  return j.dump();
}

}  // namespace protocol
}  // namespace cppsim