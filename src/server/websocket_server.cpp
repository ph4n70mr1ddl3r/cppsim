#include "websocket_server.hpp"

#include <iostream>

#include "logger.hpp"
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
    using cppsim::server::log_error;
    log_error(std::string("[WebSocketServer] Failed to open acceptor: ") + ec.message());
    return;
  }

  // Allow address reuse
  acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
  if (ec) {
    using cppsim::server::log_error;
    log_error(std::string("[WebSocketServer] Failed to set reuse_address: ") + ec.message());
    return;
  }

  // Bind to the server address
  acceptor_.bind(endpoint, ec);
  if (ec) {
    using cppsim::server::log_error;
    log_error(std::string("[WebSocketServer] Failed to bind to port ") + std::to_string(port) + ": " + ec.message());
    return;
  }

  // Start listening for connections
  acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
  if (ec) {
    using cppsim::server::log_error;
    log_error(std::string("[WebSocketServer] Failed to listen: ") + ec.message());
    return;
  }

  initialized_ = true;
  using cppsim::server::log_message;
  log_message(std::string("[WebSocketServer] Listening on port ") + std::to_string(port));
}

void websocket_server::run() {
  if (!initialized_) {
    using cppsim::server::log_error;
    log_error("[WebSocketServer] Cannot run - initialization failed");
    return;
  }
  // Start accepting connections
  do_accept();
}

void websocket_server::stop() {
  // Cancel any pending backoff timer
  if (backoff_timer_) {
    boost::beast::error_code ec;
    backoff_timer_->cancel(ec);
  }

  // Close the acceptor
  boost::beast::error_code ec;
  acceptor_.close(ec);
  if (ec) {
    using cppsim::server::log_error;
    log_error(std::string("[WebSocketServer] Error closing acceptor: ") + ec.message());
  }

  // Stop all active sessions
  if (conn_mgr_) {
      conn_mgr_->stop_all();
  }

  using cppsim::server::log_message;
  log_message("[WebSocketServer] Stopped accepting connections");
}

void websocket_server::do_accept() {
  // Accept a new connection
  acceptor_.async_accept(
      boost::asio::make_strand(ioc_),
      [this](boost::beast::error_code ec, boost::asio::ip::tcp::socket socket) {
        on_accept(ec, std::move(socket));
      });
}

void websocket_server::on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket) {
  if (ec) {
    using cppsim::server::log_error;
    log_error(std::string("[WebSocketServer] Accept failed: ") + ec.message());

    // Prevent infinite loop on persistent errors (e.g. EMFILE)
    if (!acceptor_.is_open()) {
      backoff_timer_.reset();
      return;
    }

    // Backoff: Wait 1s before retrying
    backoff_timer_ = std::make_shared<boost::asio::steady_timer>(ioc_);
    backoff_timer_->expires_after(std::chrono::seconds(1));
    backoff_timer_->async_wait([this](boost::beast::error_code timer_ec) {
      if (!timer_ec && acceptor_.is_open()) {
        do_accept();
      }
      backoff_timer_.reset();
    });
    return;
  }

  // Create a new session for this connection
  auto session = std::make_shared<websocket_session>(std::move(socket), conn_mgr_);

  // Start the session (perform WebSocket handshake and begin reading)
  session->run();

  using cppsim::server::log_message;
  log_message("[WebSocketServer] New connection accepted");

  // Accept the next connection
  do_accept();
}

}  // namespace server
}  // namespace cppsim
