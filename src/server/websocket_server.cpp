#include "websocket_server.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

#include "config.hpp"
#include "logger.hpp"
#include "websocket_session.hpp"

namespace cppsim {
namespace server {

websocket_server::~websocket_server() noexcept {
  stop();
}

websocket_server::websocket_server(boost::asio::io_context& ioc, uint16_t port,
                                       std::chrono::seconds handshake_timeout)
    : ioc_(ioc),
      acceptor_(boost::asio::make_strand(ioc)),
      conn_mgr_(std::make_shared<connection_manager>()),
      handshake_timeout_(handshake_timeout) {
  boost::beast::error_code ec;

  boost::asio::ip::tcp::endpoint endpoint{boost::asio::ip::tcp::v4(), port};
  acceptor_.open(endpoint.protocol(), ec);
  if (ec) {
    throw std::runtime_error(std::string("[WebSocketServer] Failed to open acceptor: ") + ec.message());
  }

  acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
  if (ec) {
    boost::beast::error_code close_ec;
    acceptor_.close(close_ec);
    if (close_ec) {
      log_error(std::string("[WebSocketServer] Additional error closing acceptor: ") + close_ec.message());
    }
    throw std::runtime_error(std::string("[WebSocketServer] Failed to set reuse_address: ") + ec.message());
  }

  acceptor_.bind(endpoint, ec);
  if (ec) {
    boost::beast::error_code close_ec;
    acceptor_.close(close_ec);
    if (close_ec) {
      log_error(std::string("[WebSocketServer] Additional error closing acceptor: ") + close_ec.message());
    }
    throw std::runtime_error(
        std::string("[WebSocketServer] Failed to bind to port ") + std::to_string(port) + ": " + ec.message());
  }

  acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
  if (ec) {
    boost::beast::error_code close_ec;
    acceptor_.close(close_ec);
    if (close_ec) {
      log_error(std::string("[WebSocketServer] Additional error closing acceptor: ") + close_ec.message());
    }
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
  bool expected = false;
  if (!stopped_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
    return;  // Already stopped
  }

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
    // Log construction is wrapped in try/catch because stop() is noexcept —
    // an allocation failure in string concatenation would call std::terminate.
    try {
      log_error(std::string("[WebSocketServer] Error closing acceptor: ") + ec.message());
    } catch (...) {
      // Accept error logged, but server shutdown continues regardless.
    }
  }

  // Stop all active sessions
  if (conn_mgr_) {
      conn_mgr_->stop_all();
  }

  log_message("[WebSocketServer] Stopped accepting connections");
}

void websocket_server::do_accept() {
  try {
    // shared_from_this() can throw bad_weak_ptr if run() is called when no
    // shared_ptr to this server exists.  The catch block logs and stops
    // accepting — the server is in an unrecoverable state anyway.
    auto self = shared_from_this();
    acceptor_.async_accept(
        boost::asio::make_strand(ioc_),
        [self](boost::beast::error_code ec, boost::asio::ip::tcp::socket socket) {
          self->on_accept(ec, std::move(socket));
        });
  } catch (const std::exception& e) {
    log_error(std::string("[WebSocketServer] do_accept error: ") + e.what());
  }
}

void websocket_server::on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket) {
  if (ec) {
    // Suppress noise from normal shutdown — operation_aborted fires when stop() closes the acceptor
    if (ec == boost::asio::error::operation_aborted) {
      return;
    }

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
      backoff_timer_ = std::make_shared<boost::asio::steady_timer>(acceptor_.get_executor());
      backoff_timer_->expires_after(std::chrono::seconds(backoff));
      auto weak_self = weak_from_this();
      backoff_timer_->async_wait(
          [weak_self, timer_ptr = backoff_timer_, backoff](boost::beast::error_code timer_ec) {
            auto self = weak_self.lock();
            if (!self) return;  // Server destroyed
            if (!timer_ec && self->alive_.load(std::memory_order_acquire)) {
              bool should_retry = false;
              {
                std::lock_guard<std::mutex> timer_lock(self->timer_mutex_);
                should_retry = self->acceptor_.is_open() && timer_ptr == self->backoff_timer_;
              }
              if (should_retry) {
                self->do_accept();
              }
            }
          });
    }
    // Increase backoff for next attempt, cap at configured max
    int new_backoff = std::min(backoff * 2, static_cast<int>(config::MAX_BACKOFF.count()));
    static_assert(config::MAX_BACKOFF.count() <= std::numeric_limits<int>::max(),
                  "MAX_BACKOFF must fit in int");
    backoff_seconds_.store(new_backoff, std::memory_order_release);
    return;
  }

  // Reset backoff on successful accept
  backoff_seconds_.store(1, std::memory_order_release);

  // Release the backoff timer if one was created during a previous error
  {
    std::lock_guard<std::mutex> lock(timer_mutex_);
    backoff_timer_.reset();
  }

  // Guard: don't create sessions if server is shutting down
  if (stopped_.load(std::memory_order_acquire) || !alive_.load(std::memory_order_acquire)) {
    log_message("[WebSocketServer] Discarding accepted connection during shutdown");
    return;
  }

  // Create a new session for this connection.  Wrap in try/catch so that
  // a bad_alloc from make_shared doesn't propagate through io_context::run()
  // and crash the server.  The individual connection is lost but the accept
  // loop must continue.
  try {
    auto session = std::make_shared<websocket_session>(std::move(socket), conn_mgr_, handshake_timeout_);
    session->run();
    log_message("[WebSocketServer] New connection accepted");
  } catch (const std::exception& e) {
    log_error(std::string("[WebSocketServer] Failed to create session: ") + e.what());
    // socket is moved-from on success; on exception it's still valid but we
    // let it close naturally when the temporary is destroyed.
  } catch (...) {
    log_error("[WebSocketServer] Unknown error creating session");
  }

  // Accept the next connection regardless of whether this session was created.
  do_accept();
}

}  // namespace server
}  // namespace cppsim
