#include "websocket_session.hpp"

#include <iostream>
#include <mutex>
#include <sstream>

#include "connection_manager.hpp"
#include "protocol.hpp"

namespace {
std::mutex log_mutex;

void log_message(const std::string& msg) {
  // TODO(Epic 7): Replace with centralized event bus logging (server_logger)
  std::lock_guard<std::mutex> lock(log_mutex);
  std::cout << msg << std::endl;
}


void log_error(const std::string& msg) {
  // TODO(Epic 7): Replace with centralized event bus logging (server_logger)
  std::lock_guard<std::mutex> lock(log_mutex);
  std::cerr << msg << std::endl;
}

constexpr auto HANDSHAKE_TIMEOUT = std::chrono::seconds(10);
constexpr auto IDLE_TIMEOUT = std::chrono::seconds(60);
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

void websocket_session::run() {
  // Set suggested timeout settings for the websocket
  ws_.set_option(
      boost::beast::websocket::stream_base::timeout::suggested(
          boost::beast::role_type::server));

  // Limit message size to prevent DoS (64KB)
  ws_.read_message_max(64 * 1024);

  // Start the deadline timer for authentication
  deadline_.expires_after(HANDSHAKE_TIMEOUT);
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
    std::stringstream ss;
    ss << "[WebSocketSession] Accept failed: " << ec.message();
    log_error(ss.str());
    return;
  }

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

  // Handle disconnect
  if (ec == boost::beast::websocket::error::closed) {
    std::stringstream ss;
    ss << "[WebSocketSession] Client disconnected: " << session_id_;
    log_message(ss.str());
    if (auto mgr = conn_mgr_.lock()) {
      mgr->unregister_session(session_id_);
    }
    return;
  }

  if (ec) {
    std::stringstream ss;
    ss << "[WebSocketSession] Read error for " << session_id_ << ": " << ec.message();
    log_error(ss.str());
    if (auto mgr = conn_mgr_.lock()) {
      mgr->unregister_session(session_id_);
    }
    return;
  }

  std::string message = boost::beast::buffers_to_string(buffer_.data());
  buffer_.consume(buffer_.size());

  if (state_ == state::unauthenticated) {
    auto handshake_opt = protocol::parse_handshake(message);

    if (!handshake_opt) {
      log_error("[WebSocketSession] Handshake error: Protocol error (Not HANDSHAKE)");
      // Send Error
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
      std::stringstream ss;
      ss << "[WebSocketSession] Handshake error: Incompatible version "
         << handshake_msg.protocol_version;
      log_error(ss.str());
      protocol::error_message err;
      err.error_code = protocol::error_codes::INCOMPATIBLE_VERSION;
      err.message = "Expected " + std::string(protocol::PROTOCOL_VERSION);
      send(protocol::serialize_error(err));
      close();
      return;
    }

    // Valid Handshake
    // Valid Handshake
    state_ = state::authenticated;
    
    // Store and Log Client Name if present
    if (handshake_msg.client_name) {
        log_message("[WebSocketSession] Client Name: " + *handshake_msg.client_name);
    }
    
    // Set Idle Timeout
    deadline_.expires_after(IDLE_TIMEOUT);
    check_deadline();

    // Register with connection manager
    if (auto mgr = conn_mgr_.lock()) {
      session_id_ = mgr->register_session(shared_from_this());
    } else {
      // Fallback if no manager - though constructor requires it
      log_error(
          "[WebSocketSession] Warning: No connection manager, session ID "
          "invalid");
    }

    std::stringstream ss;
    ss << "[WebSocketSession] Handshake successful for session: " << session_id_;
    log_message(ss.str());

    protocol::handshake_response resp;
    resp.session_id = session_id_;
    resp.seat_number = PLACEHOLDER_SEAT;      // Placeholder
    resp.starting_stack = PLACEHOLDER_STACK;  // Placeholder

    send(protocol::serialize_handshake_response(resp));
  } else {
    // Authenticated - just log for now
    std::stringstream ss;
    ss << "[WebSocketSession] Received from " << session_id_ << ": " << message;
    log_message(ss.str());

    // Reset Idle Timeout on activity
    deadline_.expires_after(IDLE_TIMEOUT);
    check_deadline();
  }

  // Continue reading
  do_read();
}

void websocket_session::send(const std::string& message) {
  // Post to the websocket's executor to ensure thread safety
  boost::asio::post(ws_.get_executor(),
                    [self = shared_from_this(), message]() {
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
    std::stringstream ss;
    ss << "[WebSocketSession] Write error for " << session_id_ << ": " << ec.message();
    log_error(ss.str());
    writing_ = false;
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
          if (ec != boost::asio::error::operation_aborted) {
             std::stringstream ss;
             ss << "[WebSocketSession] Timer error: " << ec.message();
             log_error(ss.str());
          }
          return;
        }

        if (self->state_ == state::unauthenticated) {
           // Timeout occurred
           std::stringstream ss;
           ss << "[WebSocketSession] Handshake timeout";
           log_error(ss.str());
           
           // Close socket directly as handshake is not complete
           self->ws_.next_layer().socket().close(ec);
        } else {
           // Idle timeout
           std::stringstream ss;
           ss << "[WebSocketSession] Idle timeout for session " << self->session_id_;
           log_error(ss.str());
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

void websocket_session::do_close() {
    // Close the websocket gracefully
  ws_.async_close(boost::beast::websocket::close_code::normal,
                  [self = shared_from_this()](boost::beast::error_code ec) {
                    if (ec) {
                      std::stringstream ss;
                      ss << "[WebSocketSession] Close error: " << ec.message();
                      log_error(ss.str());
                    }
                  });
}

}  // namespace server
}  // namespace cppsim
