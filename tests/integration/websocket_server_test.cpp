#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>

#include "server/boost_wrapper.hpp"
#include "server/websocket_server.hpp"
#include "server/connection_manager.hpp"
#include "server/config.hpp"

namespace net = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;

// Test wrapper to access private members if needed, or just standard usage
class WebSocketServerTest : public ::testing::Test {
 protected:
};

TEST(ConnectionManagerTest, BasicLifecycle) {
  auto mgr = std::make_shared<cppsim::server::connection_manager>();
  EXPECT_EQ(mgr->session_count(), 0);
  EXPECT_TRUE(mgr->active_session_ids().empty());

  // We can't easily add a session without a real socket usually,
  // but connection_manager takes shared_ptr<websocket_session>.
  // websocket_session constructor takes a socket.
  // Creating a dummy socket might be tricky without an io_context.
  // Let's rely on server test for full end-to-end.
}

TEST_F(WebSocketServerTest, AcceptsConnection) {
  net::io_context ioc_server;
  net::io_context ioc_client;
  
  uint16_t port = cppsim::server::config::DEFAULT_TEST_PORT + 1; // Use test port + 1 to avoid conflicts
  
  // Start server in a thread
  cppsim::server::websocket_server server(ioc_server, port);
  server.run();
  
  std::thread server_thread([&ioc_server] {
    ioc_server.run();
  });

  // Start client
  tcp::resolver resolver(ioc_client);
  websocket::stream<tcp::socket> ws(ioc_client);

  auto const results = resolver.resolve("localhost", std::to_string(port));
  net::connect(ws.next_layer(), results.begin(), results.end());

  // Perform handshake
  ws.handshake("localhost", "/");

  // If we got here, handshake succeeded!
  EXPECT_TRUE(ws.is_open());

  ws.write(net::buffer("Hello Server"));

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Close nicely
  ws.close(websocket::close_code::normal);
  
  server.stop();
  ioc_server.stop();
  if (server_thread.joinable()) {
    server_thread.join();
  }
}
