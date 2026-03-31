#include "websocket_server.hpp"

#include <algorithm>
#include <stdexcept>

#include "config.hpp"
#include "logger.hpp"
#include "websocket_session.hpp"

namespace cppsim {
namespace server {

websocket_server::~websocket_server() noexcept {
  stop();
}

websocket_server::websocket_server(boost::asio::io_context& ioc, uint16_t port)
    : ioc_(ioc),
      acceptor_(boost::asio::make_strand(ioc)),
      conn_mgr_(std::make_shared<connection_manager>()) {
  boost::beast::error_code ec;

  boost::asio::ip::tcp::endpoint endpoint{boost::asio::ip::tcp::v4(), port};
  acceptor_.open(endpoint.protocol(), ec);
  if (ec) {
    throw std::runtime_error(std::string("[WebSocketServer] Failed to open acceptor: ") + ec.message());
  }

  acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
  if (ec) {
    acceptor_.close(ec);
    throw std::runtime_error(std::string("[WebSocketServer] Failed to set reuse_address: ") + ec.message());
  }

  acceptor_.bind(endpoint, ec);
  if (ec) {
    acceptor_.close(ec);
    throw std::runtime_error(
        std::string("[WebSocketServer] Failed to bind to port ") + std::to_string(port) + ": " + ec.message());
  }

  acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
  if (ec) {
    acceptor_.close(ec);
    throw std::runtime_error(std::string("[WebSocketServer] Failed to listen: ") + ec.message());
  }

  initialized_.store(true, std::memory_order_release);
  log_message(std::string("[WebSocketServer] Listening on port ") + std::to_string(port));
}

void websocket_server::run() noexcept {
  if (!initialized_.load(std::memory_order_acquire)) {
    log_error("[WebSocketServer] Cannot run - initialization failed");
    return;
  }
  // Start accepting connections
  do_accept();
}

void websocket_server::stop() noexcept {
  alive_.store(false, std::memory_order_release);
  
  // Cancel any pending backoff timer
  {
    std::lock_guard<std::mutex> lock(timer_mutex_);
    if (backoff_timer_) {
      boost::beast::error_code ec;
      backoff_timer_->cancel(ec);
      backoff_timer_.reset();
    }
  }

  // Close the acceptor
  boost::beast::error_code ec;
  acceptor_.close(ec);
  if (ec) {
    log_error(std::string("[WebSocketServer] Error closing acceptor: ") + ec.message());
  }

  // Stop all active sessions
  if (conn_mgr_) {
      conn_mgr_->stop_all();
  }

  log_message("[WebSocketServer] Stopped accepting connections");
}

void websocket_server::do_accept() {
  auto self = shared_from_this();
  acceptor_.async_accept(
      boost::asio::make_strand(ioc_),
      [self](boost::beast::error_code ec, boost::asio::ip::tcp::socket socket) {
        self->on_accept(ec, std::move(socket));
      });
}

void websocket_server::on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket) {
  if (ec) {
    log_error(std::string("[WebSocketServer] Accept failed: ") + ec.message());

    // Check for fatal errors that should stop the server
    bool is_fatal = (ec == boost::asio::error::access_denied ||
                     ec == boost::asio::error::address_in_use ||
                     ec == boost::asio::error::bad_descriptor);

    if (!acceptor_.is_open() || is_fatal) {
      log_error("[WebSocketServer] Fatal accept error, stopping server");
      return;
    }

    // Exponential backoff: Wait before retrying
    int backoff = backoff_seconds_.load(std::memory_order_acquire);
    {
      std::lock_guard<std::mutex> lock(timer_mutex_);
      if (backoff_timer_) {
        boost::beast::error_code cancel_ec;
        backoff_timer_->cancel(cancel_ec);
      }
      backoff_timer_ = std::make_shared<boost::asio::steady_timer>(ioc_);
      backoff_timer_->expires_after(std::chrono::seconds(backoff));
      auto retry_self = shared_from_this();
      backoff_timer_->async_wait([retry_self, timer_ptr = backoff_timer_, backoff](boost::beast::error_code timer_ec) {
        if (!timer_ec && retry_self->alive_.load(std::memory_order_acquire)) {
          bool should_retry = false;
          {
            std::lock_guard<std::mutex> timer_lock(retry_self->timer_mutex_);
            should_retry = retry_self->acceptor_.is_open() && timer_ptr == retry_self->backoff_timer_;
          }
          if (should_retry) {
            retry_self->do_accept();
          }
        }
      });
    }
    // Increase backoff for next attempt, cap at configured max
    int new_backoff = std::min(backoff * 2, static_cast<int>(config::MAX_BACKOFF.count()));
    backoff_seconds_.store(new_backoff, std::memory_order_release);
    return;
  }

  // Reset backoff on successful accept
  backoff_seconds_.store(1, std::memory_order_release);

  // Create a new session for this connection
  auto session = std::make_shared<websocket_session>(std::move(socket), conn_mgr_);

  // Start the session (perform WebSocket handshake and begin reading)
  session->run();

  log_message("[WebSocketServer] New connection accepted");

  // Accept the next connection
  do_accept();
}

}  // namespace server
}  // namespace cppsim
