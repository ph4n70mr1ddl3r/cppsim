#include <gtest/gtest.h>
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

class WebSocketServerTest : public ::testing::Test {
 protected:
};

TEST(ConnectionManagerTest, BasicLifecycle) {
  auto mgr = std::make_shared<cppsim::server::connection_manager>();
  EXPECT_EQ(mgr->session_count(), 0);
  EXPECT_TRUE(mgr->active_session_ids().empty());
}

TEST_F(WebSocketServerTest, AcceptsConnection) {
  net::io_context ioc_server;
  net::io_context ioc_client;

  uint16_t port = cppsim::server::config::DEFAULT_TEST_PORT + 1;

  auto server = std::make_shared<cppsim::server::websocket_server>(ioc_server, port);
  server->run();

  std::thread server_thread([&ioc_server] {
    ioc_server.run();
  });

  auto start = std::chrono::steady_clock::now();
  bool connected = false;
  while (std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
    try {
      net::io_context probe_ioc;
      tcp::socket s(probe_ioc);
      tcp::resolver resolver(probe_ioc);
      auto const results = resolver.resolve("localhost", std::to_string(port));
      net::connect(s, results.begin(), results.end());
      connected = true;
      boost::system::error_code ec;
      s.close(ec);
      break;
    } catch (...) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
  ASSERT_TRUE(connected) << "Failed to connect to server within 5 seconds";

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

  server->stop();
  ioc_server.stop();
  if (server_thread.joinable()) {
    server_thread.join();
  }
}
