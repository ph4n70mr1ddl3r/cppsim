#include "protocol.hpp"

#include <gtest/gtest.h>

using namespace cppsim::protocol;

// Test: handshake_message serialization
TEST(ProtocolTest, HandshakeMessageSerialization) {
  handshake_message msg;
  msg.protocol_version = PROTOCOL_VERSION;
  msg.client_name = "test_bot";

  nlohmann::json j;
  to_json(j, msg);
  std::string json_str = j.dump();

  EXPECT_NE(json_str.find(std::string("\"protocol_version\":\"") + PROTOCOL_VERSION + "\""), std::string::npos);
  EXPECT_NE(json_str.find("\"client_name\":\"test_bot\""), std::string::npos);
}

// Test: handshake_message deserialization
TEST(ProtocolTest, HandshakeMessageDeserialization) {
  message_envelope env;
  env.message_type = message_types::HANDSHAKE;
  env.protocol_version = PROTOCOL_VERSION;
  env.payload = {{"protocol_version", PROTOCOL_VERSION}, {"client_name", "test_bot"}};

  nlohmann::json j;
  to_json(j, env);
  auto msg = parse_handshake(j.dump());

  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(msg->protocol_version, PROTOCOL_VERSION);
  ASSERT_TRUE(msg->client_name.has_value());
  EXPECT_EQ(msg->client_name.value(), "test_bot");
}

// Test: handshake_message round-trip
TEST(ProtocolTest, HandshakeMessageRoundTrip) {
  handshake_message original;
  original.protocol_version = PROTOCOL_VERSION;
  original.client_name = "bot_alpha";

  message_envelope env;
  env.message_type = message_types::HANDSHAKE;
  env.protocol_version = PROTOCOL_VERSION;
  nlohmann::json payload;
  to_json(payload, original);
  env.payload = payload;

  nlohmann::json j;
  to_json(j, env);
  auto parsed = parse_handshake(j.dump());

  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->protocol_version, original.protocol_version);
  EXPECT_EQ(parsed->client_name, original.client_name);
}

// Test: handshake_message with optional field absent
TEST(ProtocolTest, HandshakeMessageOptionalAbsent) {
  message_envelope env;
  env.message_type = message_types::HANDSHAKE;
  env.protocol_version = PROTOCOL_VERSION;
  env.payload = {{"protocol_version", PROTOCOL_VERSION}};

  nlohmann::json j;
  to_json(j, env);
  auto msg = parse_handshake(j.dump());

  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(msg->protocol_version, PROTOCOL_VERSION);
  EXPECT_FALSE(msg->client_name.has_value());
}

// Test: action_message serialization for all action types
TEST(ProtocolTest, ActionMessageFold) {
  action_message msg;
  msg.session_id = "session123";
  msg.action_type = "FOLD";
  msg.sequence_number = 1;

  nlohmann::json j;
  to_json(j, msg);
  std::string json_str = j.dump();

  EXPECT_NE(json_str.find("\"action_type\":\"FOLD\""), std::string::npos);
}

TEST(ProtocolTest, ActionMessageRaise) {
  action_message msg;
  msg.session_id = "session123";
  msg.action_type = "RAISE";
  msg.amount = 10.5;
  msg.sequence_number = 2;

  message_envelope env;
  env.message_type = message_types::ACTION;
  env.protocol_version = PROTOCOL_VERSION;
  nlohmann::json payload;
  to_json(payload, msg);
  env.payload = payload;

  nlohmann::json j;
  to_json(j, env);
  auto parsed = parse_action(j.dump());

  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->action_type, "RAISE");
  ASSERT_TRUE(parsed->amount.has_value());
  EXPECT_DOUBLE_EQ(parsed->amount.value(), 10.5);
}

// Test: state_update_message with all optional fields
TEST(ProtocolTest, StateUpdateMessageComplete) {
  state_update_message msg;
  msg.game_phase = "FLOP";
  msg.pot_size = 15.5;
  msg.current_bet = 5.0;
  msg.player_stacks = {{1, 95.0}, {2, 90.0}};
  msg.community_cards = std::vector<std::string>{"Kh", "9d", "3c"};
  msg.hole_cards = std::vector<std::string>{"As", "Kc"};
  msg.valid_actions = {"FOLD", "CALL", "RAISE"};
  msg.acting_seat = 1;

  std::string json_str = serialize_state_update(msg);
  nlohmann::json j = nlohmann::json::parse(json_str);

  // Check envelope
  EXPECT_EQ(j["message_type"], message_types::STATE_UPDATE);
  
  // Check payload
  nlohmann::json payload = j["payload"];
  EXPECT_EQ(payload["game_phase"], "FLOP");
  EXPECT_DOUBLE_EQ(payload["pot_size"], 15.5);
  EXPECT_EQ(payload["community_cards"].size(), 3);
  EXPECT_EQ(payload["hole_cards"].size(), 2);
  EXPECT_EQ(payload["acting_seat"], 1);
}

// Test: state_update_message with optional fields absent
TEST(ProtocolTest, StateUpdateMessageMinimal) {
  state_update_message msg;
  msg.game_phase = "WAITING";
  msg.pot_size = 0.0;
  msg.current_bet = 0.0;
  msg.player_stacks = {};
  msg.valid_actions = {};

  std::string json_str = serialize_state_update(msg);
  nlohmann::json j = nlohmann::json::parse(json_str);

  // Check envelope
  EXPECT_EQ(j["message_type"], message_types::STATE_UPDATE);

  // Check payload
  nlohmann::json payload = j["payload"];
  EXPECT_EQ(payload["game_phase"], "WAITING");
  EXPECT_FALSE(payload.contains("community_cards") && !payload["community_cards"].is_null());
  EXPECT_FALSE(payload.contains("hole_cards") && !payload["hole_cards"].is_null());
  EXPECT_FALSE(payload.contains("acting_seat") && !payload["acting_seat"].is_null());
}

// Test: error_message serialization
TEST(ProtocolTest, ErrorMessageSerialization) {
  error_message msg;
  msg.error_code = "INVALID_ACTION";
  msg.message = "Cannot raise with insufficient stack";
  msg.session_id = "session123";

  std::string json_str = serialize_error(msg);

  EXPECT_NE(json_str.find("\"error_code\":\"INVALID_ACTION\""),
            std::string::npos);
  EXPECT_NE(json_str.find("Cannot raise with insufficient stack"),
            std::string::npos);
}

// Test: reload_request_message round-trip
TEST(ProtocolTest, ReloadRequestRoundTrip) {
  reload_request_message original;
  original.session_id = "session456";
  original.requested_amount = 100.0;

  message_envelope env;
  env.message_type = message_types::RELOAD_REQUEST;
  env.protocol_version = PROTOCOL_VERSION;
  nlohmann::json payload;
  to_json(payload, original);
  env.payload = payload;

  nlohmann::json j;
  to_json(j, env);
  auto parsed = parse_reload_request(j.dump());

  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->session_id, original.session_id);
  EXPECT_DOUBLE_EQ(parsed->requested_amount, original.requested_amount);
}

// Test: Malformed JSON handling
TEST(ProtocolTest, MalformedJsonHandshake) {
  std::string bad_json = "{this is not valid json}";

  auto result = parse_handshake(bad_json);

  EXPECT_FALSE(result.has_value());
}

TEST(ProtocolTest, MalformedJsonAction) {
  std::string bad_json = "[]";  // Wrong type

  auto result = parse_action(bad_json);

  EXPECT_FALSE(result.has_value());
}

// Test: Missing required fields
TEST(ProtocolTest, MissingRequiredFieldsHandshake) {
  std::string json_str = R"({"client_name":"test"})";  // Missing protocol_version

  auto result = parse_handshake(json_str);

  EXPECT_FALSE(result.has_value());
}

TEST(ProtocolTest, MissingRequiredFieldsAction) {
  std::string json_str =
      R"({"session_id":"s1","action_type":"FOLD"})";  // Missing sequence_number

  auto result = parse_action(json_str);

  EXPECT_FALSE(result.has_value());
}

// Test: Invalid field types
TEST(ProtocolTest, InvalidFieldTypeAction) {
  std::string json_str =
      R"({"session_id":"s1","action_type":"RAISE","amount":"not_a_number","sequence_number":1})";

  auto result = parse_action(json_str);

  EXPECT_FALSE(result.has_value());
}

// Test: disconnect_message parsing
TEST(ProtocolTest, DisconnectMessage) {
  disconnect_message msg;
  msg.session_id = "session789";
  msg.reason = "Client timeout";

  message_envelope env;
  env.message_type = message_types::DISCONNECT;
  env.protocol_version = PROTOCOL_VERSION;
  nlohmann::json payload;
  to_json(payload, msg);
  env.payload = payload;

  nlohmann::json j;
  to_json(j, env);
  auto parsed = parse_disconnect(j.dump());

  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->session_id, "session789");
  ASSERT_TRUE(parsed->reason.has_value());
  EXPECT_EQ(parsed->reason.value(), "Client timeout");
}

// Test: message_envelope pattern
TEST(ProtocolTest, MessageEnvelope) {
  message_envelope envelope;
  envelope.message_type = message_types::HANDSHAKE;
  envelope.protocol_version = PROTOCOL_VERSION;
  envelope.payload = nlohmann::json{{"protocol_version", PROTOCOL_VERSION},
                                     {"client_name", "test_bot"}};

  nlohmann::json j;
  to_json(j, envelope);
  std::string json_str = j.dump();

  EXPECT_NE(json_str.find("\"message_type\":\"HANDSHAKE\""), std::string::npos);
  EXPECT_NE(json_str.find(std::string("\"protocol_version\":\"") + PROTOCOL_VERSION + "\""), std::string::npos);
}

// Test: Protocol version constant
TEST(ProtocolTest, ProtocolVersionConstant) {
  EXPECT_STREQ(PROTOCOL_VERSION, "v1.0");
}
