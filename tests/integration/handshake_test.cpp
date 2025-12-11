#include <gtest/gtest.h>
#include "../../src/server/boost_wrapper.hpp"
#include "../../src/server/websocket_server.hpp"
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include <iostream>
#include "protocol.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class HandshakeTest : public ::testing::Test {
protected:
    net::io_context server_ioc;
    std::thread server_thread;
    std::shared_ptr<cppsim::server::websocket_server> server;

    void SetUp() override {
        // Start server on 8080
        try {
            server = std::make_shared<cppsim::server::websocket_server>(server_ioc, 8080);
            server->run();
            
            server_thread = std::thread([this]{ 
                if (server_ioc.stopped()) server_ioc.restart();
                server_ioc.run(); 
            });
            
            // Wait for server to be ready using retry loop
            auto start = std::chrono::steady_clock::now();
            bool connected = false;
            while (std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
                try {
                    net::io_context ioc;
                    tcp::socket s(ioc);
                    tcp::resolver resolver(ioc);
                    auto const results = resolver.resolve("localhost", "8080");
                    net::connect(s, results.begin(), results.end());
                    connected = true;
                    break;
                } catch (...) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
            if (!connected) {
                FAIL() << "Failed to connect to server within 5 seconds";
            }
        } catch (const std::exception& e) {
            FAIL() << "Failed to start server: " << e.what();
        }
    }

    void TearDown() override {
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

    auto const results = resolver.resolve("localhost", "8080");
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

    auto const results = resolver.resolve("localhost", "8080");
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
        EXPECT_EQ(se.code(), websocket::error::closed);
    }
}

// Test 3: Garbage Data (Malformed)
TEST_F(HandshakeTest, MalformedData) {
    net::io_context ioc;
    tcp::resolver resolver(ioc);
    websocket::stream<tcp::socket> ws(ioc);

    auto const results = resolver.resolve("localhost", "8080");
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
        EXPECT_EQ(resp_env.payload["error_code"], cppsim::protocol::error_codes::PROTOCOL_ERROR);
    } catch (...) {
        // Closing is also acceptable
    }
}

// Test 4: Protocol Error (Wrong Message Type)
TEST_F(HandshakeTest, ProtocolError) {
    net::io_context ioc;
    tcp::resolver resolver(ioc);
    websocket::stream<tcp::socket> ws(ioc);

    auto const results = resolver.resolve("localhost", "8080");
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
    EXPECT_EQ(resp_env.payload["error_code"], cppsim::protocol::error_codes::PROTOCOL_ERROR);
}

// Test 5: Handshake Timeout
TEST_F(HandshakeTest, HandshakeTimeout) {
    net::io_context ioc;
    tcp::resolver resolver(ioc);
    websocket::stream<tcp::socket> ws(ioc);

    auto const results = resolver.resolve("localhost", "8080");
    net::connect(ws.next_layer(), results.begin(), results.end());
    perform_handshake(ws);

    // Initial read to make sure we are connected and timer starts on server
    // (Though run() starts timer immediately upon accept).
    
    // Do NOTHING for > 10 seconds.
    // We expect the server to close the connection efficiently.
    // Instead of sleep, we can try to read. The read should fail with EOF/closed eventually.
    
    beast::flat_buffer buffer;
    try {
        // This read should block until the server closes the connection
        ws.read(buffer);
        FAIL() << "Should not have received any data, connection should close";
    } catch (const beast::system_error& se) {
        // We now close the socket directly, so we might get EOF or Connection Reset
        // instead of a clean WebSocket close frame.
        bool expected = (se.code() == websocket::error::closed) ||
                        (se.code() == net::error::eof) ||
                        (se.code() == net::error::connection_reset);
        EXPECT_TRUE(expected) << "Unexpected error code: " << se.code().message() << " (" << se.code() << ")";
    }
}
