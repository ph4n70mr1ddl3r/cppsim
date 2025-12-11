#include "websocket_server.hpp"

#include <iostream>

#include "websocket_session.hpp"

namespace cppsim {
namespace server {

websocket_server::websocket_server(boost::asio::io_context& ioc, uint16_t port)
    : ioc_(ioc),
      acceptor_(boost::asio::make_strand(ioc)),
      conn_mgr_(std::make_shared<connection_manager>()) {
  boost::beast::error_code ec;

  // Open the acceptor
  boost::asio::ip::tcp::endpoint endpoint{boost::asio::ip::tcp::v4(), port};
  acceptor_.open(endpoint.protocol(), ec);
  if (ec) {
    std::cerr << "[WebSocketServer] Failed to open acceptor: " << ec.message()
              << std::endl;
    return;
  }

  // Allow address reuse
  acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
  if (ec) {
    std::cerr << "[WebSocketServer] Failed to set reuse_address: "
              << ec.message() << std::endl;
    return;
  }

  // Bind to the server address
  acceptor_.bind(endpoint, ec);
  if (ec) {
    std::cerr << "[WebSocketServer] Failed to bind to port " << port << ": "
              << ec.message() << std::endl;
    return;
  }

  // Start listening for connections
  acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
  if (ec) {
    std::cerr << "[WebSocketServer] Failed to listen: " << ec.message()
              << std::endl;
    return;
  }

  std::cout << "[WebSocketServer] Listening on port " << port << std::endl;
}

void websocket_server::run() {
  // Start accepting connections
  do_accept();
}

void websocket_server::stop() {
  // Close the acceptor
  boost::beast::error_code ec;
  acceptor_.close(ec);
  if (ec) {
    std::cerr << "[WebSocketServer] Error closing acceptor: " << ec.message()
              << std::endl;
  }
  std::cout << "[WebSocketServer] Stopped accepting connections" << std::endl;
}

void websocket_server::do_accept() {
  // Accept a new connection
  acceptor_.async_accept(
      boost::asio::make_strand(ioc_),
      [this](boost::beast::error_code ec, boost::asio::ip::tcp::socket socket) {
        on_accept(ec, std::move(socket));
      });
}



}  // namespace server
}  // namespace cppsim
