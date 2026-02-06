#include "websocket_session.hpp"

#include "config.hpp"
#include "connection_manager.hpp"
#include "logger.hpp"
#include "protocol.hpp"

namespace {
constexpr int PLACEHOLDER_SEAT = -1;
constexpr double PLACEHOLDER_STACK = 0.0;
}  // namespace

namespace cppsim {
namespace server {

websocket_session::websocket_session(
    boost::asio::ip::tcp::socket socket,
    std::shared_ptr<connection_manager> mgr)
    : ws_(std::move(socket)), conn_mgr_(mgr), deadline_(ws_.get_executor()) {
      deadline_.expires_at(boost::asio::steady_timer::time_point::max());
    }

websocket_session::~websocket_session() {
  boost::beast::error_code ec;
  deadline_.cancel(ec);
}

void websocket_session::run() {
  // Set suggested timeout settings for the websocket
  ws_.set_option(
      boost::beast::websocket::stream_base::timeout::suggested(
          boost::beast::role_type::server));

  // Limit message size to prevent DoS
  ws_.read_message_max(config::MAX_MESSAGE_SIZE);

  // Start the deadline timer for authentication
  deadline_.expires_after(config::HANDSHAKE_TIMEOUT);
  check_deadline();

  // Accept the websocket handshake
  do_accept();
}

void websocket_session::do_accept() {
  ws_.async_accept(boost::beast::bind_front_handler(
      &websocket_session::on_accept, shared_from_this()));
}

void websocket_session::on_accept(boost::beast::error_code ec) {
  if (ec) {
    using cppsim::server::log_error;
    log_error(std::string("[WebSocketSession] Accept failed: ") + ec.message());
    return;
  }

  using cppsim::server::log_message;
  log_message("[WebSocketSession] Connection accepted. Waiting for handshake...");

  // Start reading messages
  do_read();
}

void websocket_session::do_read() {
  // Read a message into our buffer
  ws_.async_read(buffer_, boost::beast::bind_front_handler(
                              &websocket_session::on_read, shared_from_this()));
}

void websocket_session::on_read(boost::beast::error_code ec,
                                 std::size_t bytes_transferred) {
  boost::ignore_unused(bytes_transferred);

  // Prevent processing if session is already closed
  if (state_.load(std::memory_order_acquire) == state::closed) {
    return;
  }

  // Handle disconnect
  if (ec == boost::beast::websocket::error::closed) {
    using cppsim::server::log_message;
    log_message(std::string("[WebSocketSession] Client disconnected: ") + session_id_);
    if (auto mgr = conn_mgr_.lock()) {
      mgr->unregister_session(session_id_);
    }
    return;
  }

  if (ec) {
    using cppsim::server::log_error;
    log_error(std::string("[WebSocketSession] Read error for ") + session_id_ + ": " + ec.message());
    if (auto mgr = conn_mgr_.lock()) {
      mgr->unregister_session(session_id_);
    }
    return;
  }

  std::string message = boost::beast::buffers_to_string(buffer_.data());
  buffer_.consume(buffer_.size());

  if (state_.load(std::memory_order_acquire) == state::unauthenticated) {
    auto handshake_opt = protocol::parse_handshake(message);

    if (!handshake_opt) {
      using cppsim::server::log_error;
      log_error("[WebSocketSession] Handshake error: Protocol error (Not HANDSHAKE)");
      protocol::error_message err;
      err.error_code = protocol::error_codes::PROTOCOL_ERROR;
      err.message = "Expected HANDSHAKE message";
      send(protocol::serialize_error(err));
      close();
      return;
    }

    const auto& handshake_msg = *handshake_opt;

    // Check Protocol Version
    if (handshake_msg.protocol_version != protocol::PROTOCOL_VERSION) {
      using cppsim::server::log_error;
      log_error(std::string("[WebSocketSession] Handshake error: Incompatible version ") + handshake_msg.protocol_version);
      protocol::error_message err;
      err.error_code = protocol::error_codes::INCOMPATIBLE_VERSION;
      err.message = "Expected " + std::string(protocol::PROTOCOL_VERSION);
      send(protocol::serialize_error(err));
      close();
      return;
    }

    // Valid Handshake
    state_.store(state::authenticated, std::memory_order_release);

    // Store and Log Client Name if present
    if (handshake_msg.client_name) {
        using cppsim::server::log_message;
        log_message(std::string("[WebSocketSession] Client Name: ") + *handshake_msg.client_name);
    }

    // Set Idle Timeout
    deadline_.expires_after(config::IDLE_TIMEOUT);
    check_deadline();

    // Register with connection manager
    if (auto mgr = conn_mgr_.lock()) {
      session_id_ = mgr->register_session(shared_from_this());
    } else {
      // Fallback if no manager - though constructor requires it
      using cppsim::server::log_error;
      log_error("[WebSocketSession] Warning: No connection manager, session ID invalid");
    }

    using cppsim::server::log_message;
    log_message(std::string("[WebSocketSession] Handshake successful for session: ") + session_id_);

    protocol::handshake_response resp;
    resp.session_id = session_id_;
    resp.seat_number = PLACEHOLDER_SEAT;      // Placeholder
    resp.starting_stack = PLACEHOLDER_STACK;  // Placeholder

    send(protocol::serialize_handshake_response(resp));
  } else {
    // Authenticated - parse and validate messages
    auto action_opt = protocol::parse_action(message);
    auto reload_opt = protocol::parse_reload_request(message);
    auto disconnect_opt = protocol::parse_disconnect(message);

    bool message_parsed = action_opt || reload_opt || disconnect_opt;

    if (!message_parsed) {
      // Log unknown but authenticated messages
      using cppsim::server::log_message;
      log_message(std::string("[WebSocketSession] Unknown message from ") + session_id_ + ": " + message);
    } else {
      // Validate session_id in message
      if (action_opt && !validate_session_id(action_opt->session_id)) {
        return;
      }
      if (reload_opt && !validate_session_id(reload_opt->session_id)) {
        return;
      }
      if (disconnect_opt && !validate_session_id(disconnect_opt->session_id)) {
        return;
      }

      // Log parsed authenticated messages
      using cppsim::server::log_message;
      log_message(std::string("[WebSocketSession] Validated message from ") + session_id_ + ": " + message);
    }

    // Reset Idle Timeout on activity
    deadline_.expires_after(config::IDLE_TIMEOUT);
    check_deadline();
  }

  // Continue reading
  do_read();
}

void websocket_session::send(const std::string& message) {
  // Post to the websocket's executor to ensure thread safety
  boost::asio::post(ws_.get_executor(),
                    [self = shared_from_this(), message]() {
                      if (self->write_queue_.size() >= config::MAX_WRITE_QUEUE_SIZE) {
                        using cppsim::server::log_error;
                        log_error(std::string("[WebSocketSession] Write queue full for session ") + self->session_id_ + ", dropping message");
                        return;
                      }
                      self->write_queue_.push(message);
                      if (!self->writing_) {
                        self->do_write();
                      }
                    });
}

void websocket_session::do_write() {
  if (write_queue_.empty()) {
    writing_ = false;
    return;
  }

  writing_ = true;
  const std::string& message = write_queue_.front();

  // Write the message
  ws_.async_write(boost::asio::buffer(message),
                  boost::beast::bind_front_handler(&websocket_session::on_write,
                                                    shared_from_this()));
}

void websocket_session::on_write(boost::beast::error_code ec,
                                  std::size_t bytes_transferred) {
  boost::ignore_unused(bytes_transferred);

  if (ec) {
    using cppsim::server::log_error;
    log_error(std::string("[WebSocketSession] Write error for ") + session_id_ + ": " + ec.message());
    writing_ = false;

    if (auto mgr = conn_mgr_.lock()) {
      mgr->unregister_session(session_id_);
    }
    return;
  }

  // Remove the sent message from queue
  write_queue_.pop();

  if (write_queue_.empty()) {
    writing_ = false;
    if (should_close_) {
      do_close();
    }
    return;
  }

  // Continue writing if more messages queued
  do_write();
}

void websocket_session::check_deadline() {
  auto self = shared_from_this();

  deadline_.async_wait(
      [self](boost::beast::error_code ec) {
        if (ec) {
           if (ec == boost::asio::error::operation_aborted) {
              // Timer was updated (expires_after called), wait again
              self->check_deadline();
              return;
           }

           if (self->state_.load(std::memory_order_acquire) == state::closed) {
              // Session is already closed, ignore timer errors
              return;
           }

           using cppsim::server::log_error;
           log_error(std::string("[WebSocketSession] Timer error: ") + ec.message());
           return;
        }

        auto current_state = self->state_.load(std::memory_order_acquire);

        if (current_state == state::closed) {
          // Session is already closed, ignore timeout
          return;
        }

        if (current_state == state::unauthenticated) {
            // Timeout occurred
            using cppsim::server::log_error;
            log_error("[WebSocketSession] Handshake timeout");

            // Close socket directly as handshake is not complete
            boost::beast::error_code socket_ec;
            self->ws_.next_layer().socket().close(socket_ec);
         } else {
            // Idle timeout
            using cppsim::server::log_error;
            log_error(std::string("[WebSocketSession] Idle timeout for session ") + self->session_id_);
            self->close();
         }
      });
}

void websocket_session::close() {
  boost::asio::post(ws_.get_executor(), [self = shared_from_this()]() {
     if (self->writing_) {
         self->should_close_ = true;
     } else {
         self->do_close();
     }
  });
}

bool websocket_session::validate_session_id(const std::string& provided_session_id) {
  if (provided_session_id != session_id_) {
    using cppsim::server::log_error;
    log_error(std::string("[WebSocketSession] Session ID mismatch: expected ") + session_id_ + ", got " +
               provided_session_id);
    protocol::error_message err;
    err.error_code = protocol::error_codes::PROTOCOL_ERROR;
    err.message = "Session ID mismatch";
    err.session_id = session_id_;
    send(protocol::serialize_error(err));
    return false;
  }
  return true;
}

void websocket_session::do_close() {
    // Close the websocket gracefully
  ws_.async_close(boost::beast::websocket::close_code::normal,
                   [self = shared_from_this()](boost::beast::error_code ec) {
                     if (ec) {
                       using cppsim::server::log_error;
                       log_error(std::string("[WebSocketSession] Close error: ") + ec.message());
                     }
                   });
}

}  // namespace server
}  // namespace cppsim
