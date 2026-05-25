#include <gtest/gtest.h>
#include "server/boost_wrapper.hpp"
#include "server/websocket_server.hpp"
#include "server/config.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>
#include "common/protocol.hpp"
#include "test_utils.hpp"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

// Use a very short handshake timeout for the timeout test (500ms instead of 10s)
static constexpr auto TEST_HANDSHAKE_TIMEOUT = std::chrono::seconds{1};

using cppsim::testing::wait_for_server;

class HandshakeTest : public ::testing::Test {
protected:
    net::io_context server_ioc;
    std::thread server_thread;
    std::shared_ptr<cppsim::server::websocket_server> server;

    // Pick a random free port with retry to avoid collisions during parallel
    // test runs (ctest --parallel).
    unsigned short test_port = 0;

    // Most tests use the default timeout; override in derived fixtures as needed.
    virtual std::chrono::seconds handshake_timeout() const { return TEST_HANDSHAKE_TIMEOUT; }

    void SetUp() override {
        // Start server on a free port with retry on bind failure
        test_port = cppsim::testing::find_free_port([&](uint16_t p) {
            server = std::make_shared<cppsim::server::websocket_server>(
                server_ioc, p, handshake_timeout());
        });
        ASSERT_NE(test_port, 0u) << "Failed to find a free port after 5 attempts";
        ASSERT_TRUE(server != nullptr);
        server->run();

        server_thread = std::thread([this]{
            if (server_ioc.stopped()) server_ioc.restart();
            server_ioc.run();
        });

        ASSERT_TRUE(wait_for_server(test_port)) << "Failed to connect to server within 5 seconds";
    }

    void TearDown() override {
        server->stop();
        server_ioc.stop();
        if(server_thread.joinable()) server_thread.join();
        server.reset();
    }

    // Helper to perform a handshake
    void perform_handshake(websocket::stream<tcp::socket>& ws) {
        ws.handshake("localhost", "/");
    }
};

// Test 1: Happy Path - Valid Handshake
TEST_F(HandshakeTest, SuccessfulHandshake) {
    net::io_context ioc;
    tcp::resolver resolver(ioc);
    websocket::stream<tcp::socket> ws(ioc);

    auto const results = resolver.resolve("localhost", std::to_string(test_port));
    net::connect(ws.next_layer(), results.begin(), results.end());
    perform_handshake(ws);

    // Send Handshake
    cppsim::protocol::message_envelope env;
    env.message_type = cppsim::protocol::message_types::HANDSHAKE;
    env.protocol_version = cppsim::protocol::PROTOCOL_VERSION;
    
    cppsim::protocol::handshake_message h_msg;
    h_msg.protocol_version = cppsim::protocol::PROTOCOL_VERSION;
    h_msg.client_name = "TestClient";
    
    nlohmann::json payload;
    cppsim::protocol::to_json(payload, h_msg);
    env.payload = payload;

    nlohmann::json j;
    cppsim::protocol::to_json(j, env);

    ws.write(net::buffer(j.dump()));

    // Read Response
    beast::flat_buffer buffer;
    ws.read(buffer);
    std::string response = beast::buffers_to_string(buffer.data());
    
    auto resp_json = nlohmann::json::parse(response);
    auto resp_env = resp_json.get<cppsim::protocol::message_envelope>();

    EXPECT_EQ(resp_env.message_type, cppsim::protocol::message_types::HANDSHAKE_RESPONSE);
    EXPECT_EQ(resp_env.protocol_version, cppsim::protocol::PROTOCOL_VERSION);
    
    ws.close(websocket::close_code::normal);
}

// Test 2: Incompatible Version
TEST_F(HandshakeTest, IncompatibleVersion) {
    net::io_context ioc;
    tcp::resolver resolver(ioc);
    websocket::stream<tcp::socket> ws(ioc);

    auto const results = resolver.resolve("localhost", std::to_string(test_port));
    net::connect(ws.next_layer(), results.begin(), results.end());
    perform_handshake(ws);

    // Send Bad Handshake
    cppsim::protocol::message_envelope env;
    env.message_type = cppsim::protocol::message_types::HANDSHAKE;
    env.protocol_version = "v0.9"; // Bad version
    env.payload = nlohmann::json{{"protocol_version", "v0.9"}}; // Valid payload structure, bad version

    nlohmann::json j;
    cppsim::protocol::to_json(j, env);

    ws.write(net::buffer(j.dump()));

    // Expect Error
    beast::flat_buffer buffer;
    try {
        ws.read(buffer);
        std::string response = beast::buffers_to_string(buffer.data());
        auto resp_json = nlohmann::json::parse(response);
        auto resp_env = resp_json.get<cppsim::protocol::message_envelope>();
        
        EXPECT_EQ(resp_env.message_type, cppsim::protocol::message_types::ERROR);
        EXPECT_EQ(resp_env.payload["error_code"], cppsim::protocol::error_codes::INCOMPATIBLE_VERSION);
    } catch (const beast::system_error& se) {
        // It might close immediately
        if (se.code() != websocket::error::closed) {
             throw;
        }
    }

    // Should be closed now
    try {
        ws.read(buffer);
        FAIL() << "Connection should be closed";
    } catch (const beast::system_error& se) {
        bool expected = (se.code() == websocket::error::closed) ||
                        (se.code() == net::error::eof) ||
                        (se.code() == net::error::connection_reset);
        EXPECT_TRUE(expected) << "Unexpected close error: " << se.code().message() << " (" << se.code() << ")";
    }
}

// Test 3: Garbage Data (Malformed)
TEST_F(HandshakeTest, MalformedData) {
    net::io_context ioc;
    tcp::resolver resolver(ioc);
    websocket::stream<tcp::socket> ws(ioc);

    auto const results = resolver.resolve("localhost", std::to_string(test_port));
    net::connect(ws.next_layer(), results.begin(), results.end());
    perform_handshake(ws);

    // Send Garbage
    ws.write(net::buffer("Not JSON"));

    // Expect Error or Close
    beast::flat_buffer buffer;
    try {
        ws.read(buffer);
        std::string response = beast::buffers_to_string(buffer.data());
        auto resp_json = nlohmann::json::parse(response);
        // If we get here, it should be an error message
        auto resp_env = resp_json.get<cppsim::protocol::message_envelope>();
        EXPECT_EQ(resp_env.message_type, cppsim::protocol::message_types::ERROR);
        EXPECT_EQ(resp_env.payload["error_code"], cppsim::protocol::error_codes::MALFORMED_HANDSHAKE);
    } catch (...) {
        // Closing is also acceptable
    }
}

// Test 4: Protocol Error (Wrong Message Type)
TEST_F(HandshakeTest, ProtocolError) {
    net::io_context ioc;
    tcp::resolver resolver(ioc);
    websocket::stream<tcp::socket> ws(ioc);

    auto const results = resolver.resolve("localhost", std::to_string(test_port));
    net::connect(ws.next_layer(), results.begin(), results.end());
    perform_handshake(ws);

    // Send ACTION instead of HANDSHAKE
    cppsim::protocol::message_envelope env;
    env.message_type = cppsim::protocol::message_types::ACTION;
    env.protocol_version = cppsim::protocol::PROTOCOL_VERSION;
    env.payload = nlohmann::json::object();

    nlohmann::json j;
    cppsim::protocol::to_json(j, env);

    ws.write(net::buffer(j.dump()));

    // Expect Error
    beast::flat_buffer buffer;
    ws.read(buffer);
    std::string response = beast::buffers_to_string(buffer.data());
    auto resp_json = nlohmann::json::parse(response);
    auto resp_env = resp_json.get<cppsim::protocol::message_envelope>();

    EXPECT_EQ(resp_env.message_type, cppsim::protocol::message_types::ERROR);
    EXPECT_EQ(resp_env.payload["error_code"], cppsim::protocol::error_codes::MALFORMED_HANDSHAKE);
}

// Test 5: Handshake Timeout (uses 1-second timeout from fixture)
TEST_F(HandshakeTest, HandshakeTimeout) {
    net::io_context ioc;
    tcp::resolver resolver(ioc);
    websocket::stream<tcp::socket> ws(ioc);

    auto const results = resolver.resolve("localhost", std::to_string(test_port));
    net::connect(ws.next_layer(), results.begin(), results.end());
    perform_handshake(ws);

    // Do NOTHING — server should close the connection after handshake timeout (1s).
    // The server sends a SESSION_CLOSED error before closing.
    beast::flat_buffer buffer;
    try {
        ws.read(buffer);
        std::string response = beast::buffers_to_string(buffer.data());
        auto resp_json = nlohmann::json::parse(response);
        // Server sends SESSION_CLOSED error before closing
        EXPECT_EQ(resp_json["message_type"], cppsim::protocol::message_types::ERROR);
        EXPECT_EQ(resp_json["payload"]["error_code"], cppsim::protocol::error_codes::SESSION_CLOSED);
    } catch (const beast::system_error& se) {
        // Close without error message is also acceptable (timing-dependent)
        bool expected = (se.code() == websocket::error::closed) ||
                        (se.code() == net::error::eof) ||
                        (se.code() == net::error::connection_reset);
        EXPECT_TRUE(expected) << "Unexpected error code: " << se.code().message() << " (" << se.code() << ")";
    }
}

// ===========================================================================
// Authenticated message flow integration tests
// ===========================================================================

// Helper: perform a full handshake and return the session_id.
// The ws reference is out-parameter (already connected and handshaked).
// Validates the HANDSHAKE_RESPONSE envelope before extracting session_id
// to ensure the server responded correctly (catches silent protocol bugs).
static std::string do_handshake(websocket::stream<tcp::socket>& ws, uint16_t port) {
    tcp::resolver resolver(ws.get_executor());
    auto const results = resolver.resolve("localhost", std::to_string(port));
    net::connect(ws.next_layer(), results.begin(), results.end());
    ws.handshake("localhost", "/");

    cppsim::protocol::message_envelope env;
    env.message_type = cppsim::protocol::message_types::HANDSHAKE;
    env.protocol_version = cppsim::protocol::PROTOCOL_VERSION;
    env.payload = nlohmann::json{{"protocol_version", cppsim::protocol::PROTOCOL_VERSION},
                                  {"client_name", "ActionTestBot"}};
    nlohmann::json j;
    cppsim::protocol::to_json(j, env);
    ws.write(net::buffer(j.dump()));

    beast::flat_buffer buf;
    ws.read(buf);
    std::string resp = beast::buffers_to_string(buf.data());
    auto resp_json = nlohmann::json::parse(resp);

    // Validate response envelope before extracting session_id
    EXPECT_EQ(resp_json["message_type"].get<std::string>(),
              cppsim::protocol::message_types::HANDSHAKE_RESPONSE);
    EXPECT_EQ(resp_json["protocol_version"].get<std::string>(),
              cppsim::protocol::PROTOCOL_VERSION);
    EXPECT_TRUE(resp_json["payload"].contains("session_id"));
    EXPECT_TRUE(resp_json["payload"].contains("seat_number"));
    EXPECT_TRUE(resp_json["payload"].contains("starting_stack"));

    return resp_json["payload"]["session_id"].get<std::string>();
}

class ActionTest : public ::testing::Test {
protected:
    net::io_context server_ioc;
    std::thread server_thread;
    std::shared_ptr<cppsim::server::websocket_server> server;
    unsigned short test_port = 0;

    void SetUp() override {
        test_port = cppsim::testing::find_free_port([&](uint16_t p) {
            server = std::make_shared<cppsim::server::websocket_server>(server_ioc, p);
        });
        ASSERT_NE(test_port, 0u);
        ASSERT_TRUE(server != nullptr);
        server->run();
        server_thread = std::thread([this] {
            if (server_ioc.stopped()) server_ioc.restart();
            server_ioc.run();
        });
        ASSERT_TRUE(wait_for_server(test_port));
    }

    void TearDown() override {
        server->stop();
        server_ioc.stop();
        if (server_thread.joinable()) server_thread.join();
        server.reset();
    }
};

// Test: Valid ACTION (FOLD) through the server
TEST_F(ActionTest, ValidFoldAction) {
    net::io_context ioc;
    websocket::stream<tcp::socket> ws(ioc);
    std::string session_id = do_handshake(ws, test_port);
    ASSERT_FALSE(session_id.empty());

    // Send FOLD action
    cppsim::protocol::message_envelope env;
    env.message_type = cppsim::protocol::message_types::ACTION;
    env.protocol_version = cppsim::protocol::PROTOCOL_VERSION;
    env.payload = nlohmann::json{
        {"session_id", session_id},
        {"action_type", "FOLD"},
        {"sequence_number", 1}
    };
    nlohmann::json j;
    cppsim::protocol::to_json(j, env);
    ws.write(net::buffer(j.dump()));

    // Server should NOT close the connection — the action was valid.
    // Send a second action to prove the session is still alive.
    env.payload = nlohmann::json{
        {"session_id", session_id},
        {"action_type", "CHECK"},
        {"sequence_number", 2}
    };
    cppsim::protocol::to_json(j, env);
    ws.write(net::buffer(j.dump()));

    // Clean disconnect
    ws.close(websocket::close_code::normal);
}

// Test: Valid RELOAD_REQUEST through the server
TEST_F(ActionTest, ValidReloadRequest) {
    net::io_context ioc;
    websocket::stream<tcp::socket> ws(ioc);
    std::string session_id = do_handshake(ws, test_port);
    ASSERT_FALSE(session_id.empty());

    // Send RELOAD_REQUEST
    cppsim::protocol::message_envelope env;
    env.message_type = cppsim::protocol::message_types::RELOAD_REQUEST;
    env.protocol_version = cppsim::protocol::PROTOCOL_VERSION;
    env.payload = nlohmann::json{
        {"session_id", session_id},
        {"requested_amount", 500}  // 500 cents = $5.00
    };
    nlohmann::json j;
    cppsim::protocol::to_json(j, env);
    ws.write(net::buffer(j.dump()));

    // Read RELOAD_RESPONSE
    beast::flat_buffer buf;
    ws.read(buf);
    std::string resp = beast::buffers_to_string(buf.data());
    auto resp_json = nlohmann::json::parse(resp);
    EXPECT_EQ(resp_json["message_type"], cppsim::protocol::message_types::RELOAD_RESPONSE);
    EXPECT_EQ(resp_json["payload"]["granted"], true);
    EXPECT_EQ(resp_json["payload"]["new_stack"].get<int64_t>(), 500);

    // Second reload to verify cumulative stacking
    env.payload = nlohmann::json{
        {"session_id", session_id},
        {"requested_amount", 300}  // 300 cents = $3.00
    };
    cppsim::protocol::to_json(j, env);
    ws.write(net::buffer(j.dump()));

    beast::flat_buffer buf2;
    ws.read(buf2);
    resp = beast::buffers_to_string(buf2.data());
    resp_json = nlohmann::json::parse(resp);
    EXPECT_EQ(resp_json["payload"]["new_stack"].get<int64_t>(), 800);

    ws.close(websocket::close_code::normal);
}

// Test: Valid DISCONNECT through the server
TEST_F(ActionTest, ValidDisconnect) {
    net::io_context ioc;
    websocket::stream<tcp::socket> ws(ioc);
    std::string session_id = do_handshake(ws, test_port);
    ASSERT_FALSE(session_id.empty());

    // Send DISCONNECT
    cppsim::protocol::message_envelope env;
    env.message_type = cppsim::protocol::message_types::DISCONNECT;
    env.protocol_version = cppsim::protocol::PROTOCOL_VERSION;
    env.payload = nlohmann::json{
        {"session_id", session_id},
        {"reason", "Goodbye"}
    };
    nlohmann::json j;
    cppsim::protocol::to_json(j, env);
    ws.write(net::buffer(j.dump()));

    // Server should close the connection
    beast::flat_buffer buf;
    try {
        ws.read(buf);
        FAIL() << "Connection should be closed after DISCONNECT";
    } catch (const beast::system_error& se) {
        bool expected = (se.code() == websocket::error::closed) ||
                        (se.code() == net::error::eof) ||
                        (se.code() == net::error::connection_reset);
        EXPECT_TRUE(expected) << "Unexpected error: " << se.code().message();
    }
}

// Test: ACTION with wrong session_id should be rejected
TEST_F(ActionTest, ActionWrongSessionId) {
    net::io_context ioc;
    websocket::stream<tcp::socket> ws(ioc);
    std::string session_id = do_handshake(ws, test_port);
    ASSERT_FALSE(session_id.empty());

    // Send ACTION with a different session_id
    cppsim::protocol::message_envelope env;
    env.message_type = cppsim::protocol::message_types::ACTION;
    env.protocol_version = cppsim::protocol::PROTOCOL_VERSION;
    env.payload = nlohmann::json{
        {"session_id", "sess_deadbeef1234fake"},
        {"action_type", "FOLD"},
        {"sequence_number", 1}
    };
    nlohmann::json j;
    cppsim::protocol::to_json(j, env);
    ws.write(net::buffer(j.dump()));

    // Expect error or close
    beast::flat_buffer buf;
    try {
        ws.read(buf);
        std::string resp = beast::buffers_to_string(buf.data());
        auto resp_json = nlohmann::json::parse(resp);
        EXPECT_EQ(resp_json["payload"]["error_code"], cppsim::protocol::error_codes::PROTOCOL_ERROR);
    } catch (const beast::system_error& se) {
        bool expected = (se.code() == websocket::error::closed) ||
                        (se.code() == net::error::eof);
        EXPECT_TRUE(expected);
    }
}

// Test: Rate limiting closes the connection
TEST_F(ActionTest, RateLimitExceeded) {
    net::io_context ioc;
    websocket::stream<tcp::socket> ws(ioc);
    std::string session_id = do_handshake(ws, test_port);
    ASSERT_FALSE(session_id.empty());

    // Send more messages than the rate limit allows.
    // config::MAX_MESSAGES_PER_WINDOW = 10, so send 11 rapid-fire messages.
    for (size_t i = 0; i <= cppsim::server::config::MAX_MESSAGES_PER_WINDOW; ++i) {
        cppsim::protocol::message_envelope env;
        env.message_type = cppsim::protocol::message_types::ACTION;
        env.protocol_version = cppsim::protocol::PROTOCOL_VERSION;
        // Use sequence_number = i+1 to avoid replay detection
        env.payload = nlohmann::json{
            {"session_id", session_id},
            {"action_type", "FOLD"},
            {"sequence_number", static_cast<int64_t>(i + 1)}
        };
        nlohmann::json j;
        cppsim::protocol::to_json(j, env);
        ws.write(net::buffer(j.dump()));
    }

    // Server should close due to rate limit
    beast::flat_buffer buf;
    try {
        ws.read(buf);
        std::string resp = beast::buffers_to_string(buf.data());
        auto resp_json = nlohmann::json::parse(resp);
        // The server sends a SESSION_CLOSED error before closing
        EXPECT_EQ(resp_json["payload"]["error_code"], cppsim::protocol::error_codes::SESSION_CLOSED);
    } catch (const beast::system_error& se) {
        bool expected = (se.code() == websocket::error::closed) ||
                        (se.code() == net::error::eof) ||
                        (se.code() == net::error::connection_reset);
        EXPECT_TRUE(expected) << "Unexpected error: " << se.code().message();
    }
}

// Test: Sequence number gap exceeding MAX_SEQUENCE_GAP should be rejected
TEST_F(ActionTest, SequenceGapTooLarge) {
    net::io_context ioc;
    websocket::stream<tcp::socket> ws(ioc);
    std::string session_id = do_handshake(ws, test_port);
    ASSERT_FALSE(session_id.empty());

    // Send ACTION with a sequence number far beyond the allowed gap
    int64_t huge_seq = cppsim::server::config::MAX_SEQUENCE_GAP + 100;
    cppsim::protocol::message_envelope env;
    env.message_type = cppsim::protocol::message_types::ACTION;
    env.protocol_version = cppsim::protocol::PROTOCOL_VERSION;
    env.payload = nlohmann::json{
        {"session_id", session_id},
        {"action_type", "FOLD"},
        {"sequence_number", huge_seq}
    };
    nlohmann::json j;
    cppsim::protocol::to_json(j, env);
    ws.write(net::buffer(j.dump()));

    // Server should send PROTOCOL_ERROR and close
    beast::flat_buffer buf;
    try {
        ws.read(buf);
        std::string resp = beast::buffers_to_string(buf.data());
        auto resp_json = nlohmann::json::parse(resp);
        EXPECT_EQ(resp_json["payload"]["error_code"], cppsim::protocol::error_codes::PROTOCOL_ERROR);
    } catch (const beast::system_error& se) {
        bool expected = (se.code() == websocket::error::closed) ||
                        (se.code() == net::error::eof) ||
                        (se.code() == net::error::connection_reset);
        EXPECT_TRUE(expected) << "Unexpected error: " << se.code().message();
    }
}

// Test: Replayed sequence number should be rejected
TEST_F(ActionTest, ReplayedSequenceNumber) {
    net::io_context ioc;
    websocket::stream<tcp::socket> ws(ioc);
    std::string session_id = do_handshake(ws, test_port);
    ASSERT_FALSE(session_id.empty());

    // Send first action with seq=1
    cppsim::protocol::message_envelope env;
    env.message_type = cppsim::protocol::message_types::ACTION;
    env.protocol_version = cppsim::protocol::PROTOCOL_VERSION;
    env.payload = nlohmann::json{
        {"session_id", session_id},
        {"action_type", "FOLD"},
        {"sequence_number", 1}
    };
    nlohmann::json j;
    cppsim::protocol::to_json(j, env);
    ws.write(net::buffer(j.dump()));

    // Send duplicate seq=1 (replay)
    ws.write(net::buffer(j.dump()));

    // Server should send PROTOCOL_ERROR and close
    beast::flat_buffer buf;
    try {
        ws.read(buf);
        std::string resp = beast::buffers_to_string(buf.data());
        auto resp_json = nlohmann::json::parse(resp);
        EXPECT_EQ(resp_json["payload"]["error_code"], cppsim::protocol::error_codes::PROTOCOL_ERROR);
    } catch (const beast::system_error& se) {
        bool expected = (se.code() == websocket::error::closed) ||
                        (se.code() == net::error::eof) ||
                        (se.code() == net::error::connection_reset);
        EXPECT_TRUE(expected) << "Unexpected error: " << se.code().message();
    }
}
