#include <gtest/gtest.h>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include "server/boost_wrapper.hpp"
#include "server/websocket_server.hpp"
#include "server/connection_manager.hpp"
#include "server/config.hpp"
#include "common/protocol.hpp"
#include <nlohmann/json.hpp>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;

// Wait for a TCP server to become reachable by polling with probe connections.
// Returns true on success, false on timeout.
static bool wait_for_server(uint16_t port,
                             std::chrono::steady_clock::duration timeout = std::chrono::seconds(5)) {
  auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    try {
      net::io_context ioc;
      tcp::socket s(ioc);
      tcp::resolver resolver(ioc);
      auto const results = resolver.resolve("localhost", std::to_string(port));
      net::connect(s, results.begin(), results.end());
      boost::system::error_code ec;
      s.close(ec);
      return true;
    } catch (...) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
  return false;
}

TEST(ConnectionManagerTest, BasicLifecycle) {
  auto mgr = std::make_shared<cppsim::server::connection_manager>();
  EXPECT_EQ(mgr->session_count(), 0);
  EXPECT_TRUE(mgr->active_session_ids().empty());
}

TEST(ConnectionManagerTest, StopAllOnEmpty) {
  auto mgr = std::make_shared<cppsim::server::connection_manager>();
  mgr->stop_all();  // Should not crash or log errors
  EXPECT_EQ(mgr->session_count(), 0);
}

TEST(ConnectionManagerTest, UnregisterUnknownSession) {
  auto mgr = std::make_shared<cppsim::server::connection_manager>();
  mgr->unregister_session("sess_nonexistent");  // Should not crash
  EXPECT_EQ(mgr->session_count(), 0);
}

TEST(ConnectionManagerTest, GetNonexistentSession) {
  auto mgr = std::make_shared<cppsim::server::connection_manager>();
  auto session = mgr->get_session("sess_nonexistent");
  EXPECT_EQ(session, nullptr);
}

TEST(WebSocketServerTest, AcceptsConnection) {
  net::io_context ioc_server;
  net::io_context ioc_client;

  uint16_t port = cppsim::server::config::DEFAULT_TEST_PORT + 1;

  auto server = std::make_shared<cppsim::server::websocket_server>(ioc_server, port);
  server->run();

  std::thread server_thread([&ioc_server] {
    ioc_server.run();
  });

  struct cleanup_guard {
    std::function<void()> fn;
    ~cleanup_guard() { if (fn) fn(); }
  } guard{[&]() {
    server->stop();
    ioc_server.stop();
    if (server_thread.joinable()) {
      server_thread.join();
    }
  }};

  ASSERT_TRUE(wait_for_server(port)) << "Failed to connect to server within 5 seconds";

  tcp::resolver resolver(ioc_client);
  websocket::stream<tcp::socket> ws(ioc_client);

  auto const results = resolver.resolve("localhost", std::to_string(port));
  net::connect(ws.next_layer(), results.begin(), results.end());

  ws.handshake("localhost", "/");

  EXPECT_TRUE(ws.is_open());

  cppsim::protocol::message_envelope env;
  env.message_type = cppsim::protocol::message_types::HANDSHAKE;
  env.protocol_version = cppsim::protocol::PROTOCOL_VERSION;
  env.payload = nlohmann::json{{"protocol_version", cppsim::protocol::PROTOCOL_VERSION},
                                {"client_name", "TestBot"}};

  nlohmann::json j;
  cppsim::protocol::to_json(j, env);
  ws.write(net::buffer(j.dump()));

  beast::flat_buffer buffer;
  ws.read(buffer);
  std::string response = beast::buffers_to_string(buffer.data());

  auto resp_json = nlohmann::json::parse(response);
  EXPECT_EQ(resp_json["message_type"], cppsim::protocol::message_types::HANDSHAKE_RESPONSE);
  EXPECT_TRUE(resp_json["payload"].contains("session_id"));
  EXPECT_FALSE(resp_json["payload"]["session_id"].get<std::string>().empty());

  ws.close(websocket::close_code::normal);
}

TEST(WebSocketServerTest, StopIsIdempotent) {
  net::io_context ioc;
  uint16_t port = cppsim::server::config::DEFAULT_TEST_PORT + 2;

  auto server = std::make_shared<cppsim::server::websocket_server>(ioc, port);
  server->run();

  std::thread server_thread([&ioc] { ioc.run(); });

  ASSERT_TRUE(wait_for_server(port));

  server->stop();
  server->stop();  // Second call should be a no-op
  server->stop();  // Third call should also be a no-op

  ioc.stop();
  if (server_thread.joinable()) server_thread.join();
}
