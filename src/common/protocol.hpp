#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace cppsim {
namespace protocol {

// Protocol version constant
constexpr const char* PROTOCOL_VERSION = "v1.0";

// Error Codes
namespace error_codes {
constexpr const char* INCOMPATIBLE_VERSION = "INCOMPATIBLE_VERSION";
constexpr const char* PROTOCOL_VERSION_MISMATCH = "PROTOCOL_VERSION_MISMATCH"; // Use this or INCOMPATIBLE_VERSION? Kept INCOMPATIBLE_VERSION per story
constexpr const char* PROTOCOL_ERROR = "PROTOCOL_ERROR";
constexpr const char* MALFORMED_HANDSHAKE = "MALFORMED_HANDSHAKE";
constexpr const char* MALFORMED_MESSAGE = "MALFORMED_MESSAGE";
}

// Player stack information for state updates
struct player_stack {
  int seat;
  double stack;
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
  double starting_stack;
};

// ACTION message - Client sends poker action
struct action_message {
  std::string session_id;
  std::string action_type;  // "FOLD", "CALL", "RAISE", "CHECK", "ALL_IN"
  std::optional<double> amount;
  int sequence_number;
};

// STATE_UPDATE message - Server broadcasts game state
struct state_update_message {
  std::string game_phase;  // "WAITING", "PREFLOP", "FLOP", "TURN", "RIVER",
                           // "SHOWDOWN", "HAND_COMPLETE"
  double pot_size;
  double current_bet;
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
  double requested_amount;
};

// RELOAD_RESPONSE message - Server responds to reload request
struct reload_response_message {
  bool granted;
  double new_stack;
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
std::optional<handshake_message> parse_handshake(const std::string& json_str);
std::optional<action_message> parse_action(const std::string& json_str);
std::optional<reload_request_message> parse_reload_request(const std::string& json_str);
std::optional<disconnect_message> parse_disconnect(const std::string& json_str);

// Serialization functions - convert messages to JSON strings
std::string serialize_state_update(const state_update_message& msg);
std::string serialize_error(const error_message& msg);
std::string serialize_handshake_response(const handshake_response& msg);
std::string serialize_reload_response(const reload_response_message& msg);

// Protocols definitions

// nlohmann/json serialization functions MUST be in global namespace
// Manual to_json/from_json for proper std::optional support

inline void to_json(nlohmann::json& j, const cppsim::protocol::player_stack& p) {
  j = nlohmann::json{{"seat", p.seat}, {"stack", p.stack}};
}

inline void from_json(const nlohmann::json& j, cppsim::protocol::player_stack& p) {
  j.at("seat").get_to(p.seat);
  j.at("stack").get_to(p.stack);
}

inline void to_json(nlohmann::json& j, const cppsim::protocol::handshake_message& m) {
  j = nlohmann::json{{"protocol_version", m.protocol_version}};
  if (m.client_name) j["client_name"] = *m.client_name;
}

inline void from_json(const nlohmann::json& j, cppsim::protocol::handshake_message& m) {
  j.at("protocol_version").get_to(m.protocol_version);
  if (j.contains("client_name") && !j["client_name"].is_null()) {
    m.client_name = j["client_name"].get<std::string>();
  }
}

inline void to_json(nlohmann::json& j, const cppsim::protocol::handshake_response& m) {
  j = nlohmann::json{{"session_id", m.session_id},
                     {"seat_number", m.seat_number},
                     {"starting_stack", m.starting_stack}};
}

inline void from_json(const nlohmann::json& j, cppsim::protocol::handshake_response& m) {
  j.at("session_id").get_to(m.session_id);
  j.at("seat_number").get_to(m.seat_number);
  j.at("starting_stack").get_to(m.starting_stack);
}

inline void to_json(nlohmann::json& j, const cppsim::protocol::action_message& m) {
  j = nlohmann::json{{"session_id", m.session_id},
                     {"action_type", m.action_type},
                     {"sequence_number", m.sequence_number}};
  if (m.amount) j["amount"] = *m.amount;
}

inline void from_json(const nlohmann::json& j, cppsim::protocol::action_message& m) {
  j.at("session_id").get_to(m.session_id);
  j.at("action_type").get_to(m.action_type);
  j.at("sequence_number").get_to(m.sequence_number);
  if (j.contains("amount") && !j["amount"].is_null()) {
    m.amount = j["amount"].get<double>();
  }
}

inline void to_json(nlohmann::json& j, const cppsim::protocol::state_update_message& m) {
  j = nlohmann::json::object();
  j["game_phase"] = m.game_phase;
  j["pot_size"] = m.pot_size;
  j["current_bet"] = m.current_bet;
  
  // Manually convert player_stacks vector to JSON array
  nlohmann::json stacks_array = nlohmann::json::array();
  for (const auto& ps : m.player_stacks) {
    nlohmann::json ps_json;
    to_json(ps_json, ps);
    stacks_array.push_back(ps_json);
  }
  j["player_stacks"] = stacks_array;
  
  j["valid_actions"] = m.valid_actions;
  if (m.community_cards) j["community_cards"] = *m.community_cards;
  if (m.hole_cards) j["hole_cards"] = *m.hole_cards;
  if (m.acting_seat) j["acting_seat"] = *m.acting_seat;
}

inline void from_json(const nlohmann::json& j, cppsim::protocol::state_update_message& m) {
  j.at("game_phase").get_to(m.game_phase);
  j.at("pot_size").get_to(m.pot_size);
  j.at("current_bet").get_to(m.current_bet);
  
  // Manually parse player_stacks array
  const auto& stacks_json = j.at("player_stacks");
  m.player_stacks.clear();
  for (const auto& stack_json : stacks_json) {
    cppsim::protocol::player_stack ps;
    from_json(stack_json, ps);
    m.player_stacks.push_back(ps);
  }
  
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

inline void to_json(nlohmann::json& j, const cppsim::protocol::error_message& m) {
  j = nlohmann::json{{"error_code", m.error_code}, {"message", m.message}};
  if (m.session_id) j["session_id"] = *m.session_id;
}

inline void from_json(const nlohmann::json& j, cppsim::protocol::error_message& m) {
  j.at("error_code").get_to(m.error_code);
  j.at("message").get_to(m.message);
  if (j.contains("session_id") && !j["session_id"].is_null()) {
    m.session_id = j["session_id"].get<std::string>();
  }
}

inline void to_json(nlohmann::json& j, const cppsim::protocol::reload_request_message& m) {
  j = nlohmann::json{{"session_id", m.session_id}, {"requested_amount", m.requested_amount}};
}

inline void from_json(const nlohmann::json& j, cppsim::protocol::reload_request_message& m) {
  j.at("session_id").get_to(m.session_id);
  j.at("requested_amount").get_to(m.requested_amount);
}

inline void to_json(nlohmann::json& j, const cppsim::protocol::reload_response_message& m) {
  j = nlohmann::json{{"granted", m.granted}, {"new_stack", m.new_stack}};
}

inline void from_json(const nlohmann::json& j, cppsim::protocol::reload_response_message& m) {
  j.at("granted").get_to(m.granted);
  j.at("new_stack").get_to(m.new_stack);
}

inline void to_json(nlohmann::json& j, const cppsim::protocol::disconnect_message& m) {
  j = nlohmann::json{{"session_id", m.session_id}};
  if (m.reason) j["reason"] = *m.reason;
}

inline void from_json(const nlohmann::json& j, cppsim::protocol::disconnect_message& m) {
  j.at("session_id").get_to(m.session_id);
  if (j.contains("reason") && !j["reason"].is_null()) {
    m.reason = j["reason"].get<std::string>();
  }
}

inline void to_json(nlohmann::json& j, const cppsim::protocol::message_envelope& m) {
  j = nlohmann::json{
      {"message_type", m.message_type}, {"protocol_version", m.protocol_version}, {"payload", m.payload}};
}

inline void from_json(const nlohmann::json& j, cppsim::protocol::message_envelope& m) {
  j.at("message_type").get_to(m.message_type);
  j.at("protocol_version").get_to(m.protocol_version);
  j.at("payload").get_to(m.payload);
}

}  // namespace protocol
}  // namespace cppsim
