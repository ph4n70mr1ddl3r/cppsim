#include "websocket_session.hpp"

#include "config.hpp"
#include "connection_manager.hpp"
#include "logger.hpp"
#include "protocol.hpp"



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
    cppsim::server::log_error(std::string("[WebSocketSession] Accept failed: ") + ec.message());
    return;
  }

  cppsim::server::log_message("[WebSocketSession] Connection accepted. Waiting for handshake...");

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

  auto current_state = state_.load(std::memory_order_acquire);
  
  // Prevent processing if session is already closed
  if (current_state == state::closed) {
    return;
  }

  // Handle disconnect
  if (ec == boost::beast::websocket::error::closed) {
    std::string session_id_copy = get_session_id_safe();
    cppsim::server::log_message(std::string("[WebSocketSession] Client disconnected: ") + session_id_copy);
    if (auto mgr = conn_mgr_.lock()) {
      mgr->unregister_session(session_id_copy);
    }
    return;
  }

  if (ec) {
    std::string session_id_copy = get_session_id_safe();
    cppsim::server::log_error(std::string("[WebSocketSession] Read error for ") + session_id_copy + ": " + ec.message());
    if (auto mgr = conn_mgr_.lock()) {
      mgr->unregister_session(session_id_copy);
    }
    return;
  }

  std::string message = boost::beast::buffers_to_string(buffer_.data());
  buffer_.consume(buffer_.size());

  auto now = std::chrono::steady_clock::now();

  // Sliding window rate limiting (mutex-protected)
  {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);
    auto window_start = now - RATE_LIMIT_WINDOW;
    auto it = std::remove_if(message_timestamps_.begin(), message_timestamps_.end(),
        [window_start](const auto& timestamp) { return timestamp < window_start; });
    message_timestamps_.erase(it, message_timestamps_.end());

    // Check if rate limit exceeded
    if (message_timestamps_.size() >= static_cast<size_t>(config::MAX_MESSAGES_PER_SECOND)) {
      cppsim::server::log_error("[WebSocketSession] Rate limit exceeded for session " + get_session_id_safe());
      close();
      return;
    }

    // Add current message timestamp, limit memory growth
    if (message_timestamps_.size() >= config::MAX_TIMESTAMPS_TO_TRACK) {
      message_timestamps_.erase(message_timestamps_.begin());
    }
    message_timestamps_.push_back(now);
  }

  if (current_state == state::unauthenticated) {
    auto handshake_opt = protocol::parse_handshake(message);

    if (!handshake_opt) {
      cppsim::server::log_error("[WebSocketSession] Handshake error: Protocol error (Not HANDSHAKE)");
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
      cppsim::server::log_error(std::string("[WebSocketSession] Handshake error: Incompatible version ") + handshake_msg.protocol_version);
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
        cppsim::server::log_message(std::string("[WebSocketSession] Client Name: ") + *handshake_msg.client_name);
    }

    // Set Idle Timeout
    deadline_.expires_after(config::IDLE_TIMEOUT);
    check_deadline();

    // Register with connection manager
    std::string new_session_id;
    if (auto mgr = conn_mgr_.lock()) {
      new_session_id = mgr->register_session(shared_from_this());
      if (new_session_id.empty()) {
        cppsim::server::log_error("[WebSocketSession] Failed to register session - ID collision");
        protocol::error_message err;
        err.error_code = protocol::error_codes::PROTOCOL_ERROR;
        err.message = "Failed to generate unique session ID";
        send(protocol::serialize_error(err));
        close();
        return;
      }
    } else {
      // Fallback if no manager - though constructor requires it
      cppsim::server::log_error("[WebSocketSession] Warning: No connection manager, session ID invalid");
      protocol::error_message err;
      err.error_code = protocol::error_codes::PROTOCOL_ERROR;
      err.message = "Connection manager not available";
      send(protocol::serialize_error(err));
      close();
      return;
    }

    {
      std::lock_guard<std::mutex> lock(session_id_mutex_);
      session_id_ = new_session_id;
    }

    cppsim::server::log_message(std::string("[WebSocketSession] Handshake successful for session: ") + new_session_id);

    protocol::handshake_response resp;
    resp.session_id = new_session_id;
    resp.seat_number = config::PLACEHOLDER_SEAT;
    resp.starting_stack = config::PLACEHOLDER_STACK;

    send(protocol::serialize_handshake_response(resp));
  } else {
    // Authenticated - parse and validate messages
    auto action_opt = protocol::parse_action(message);
    auto reload_opt = protocol::parse_reload_request(message);
    auto disconnect_opt = protocol::parse_disconnect(message);

    bool message_parsed = action_opt || reload_opt || disconnect_opt;

    if (!message_parsed) {
      // Log unknown but authenticated messages
      cppsim::server::log_message(std::string("[WebSocketSession] Unknown message from ") + get_session_id_safe() + ": " + message);
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

      // Validate sequence number for action messages (replay attack prevention)
      if (action_opt) {
        int last_seq = last_sequence_number_.load(std::memory_order_acquire);
        if (action_opt->sequence_number <= last_seq) {
          cppsim::server::log_error("[WebSocketSession] Invalid sequence number " +
                    std::to_string(action_opt->sequence_number) + " (expected > " +
                    std::to_string(last_seq) + ")");
          protocol::error_message err;
          err.error_code = protocol::error_codes::PROTOCOL_ERROR;
          err.message = "Invalid sequence number - possible replay attack";
          send(protocol::serialize_error(err));
          return;
        }
        last_sequence_number_.store(action_opt->sequence_number, std::memory_order_release);
      }

      // Log parsed authenticated messages
      cppsim::server::log_message(std::string("[WebSocketSession] Validated message from ") + get_session_id_safe() + ": " + message);
    }

    // Reset Idle Timeout on activity
    deadline_.expires_after(config::IDLE_TIMEOUT);
    check_deadline();
  }

  // Continue reading
  do_read();
}

void websocket_session::queue_message(std::string&& message) {
  {
    std::lock_guard<std::mutex> lock(write_queue_mutex_);
    if (write_queue_.size() >= config::MAX_WRITE_QUEUE_SIZE) {
      cppsim::server::log_error(std::string("[WebSocketSession] Write queue full for session ") + get_session_id_safe() + ", dropping message");
      return;
    }
    write_queue_.push(std::move(message));
  }

  boost::asio::post(ws_.get_executor(),
                    [self = shared_from_this()]() mutable {
                       if (!self->writing_.load(std::memory_order_acquire)) {
                         self->do_write();
                       }
                    });
}

void websocket_session::send(const std::string& message) {
  queue_message(std::string(message));
}

void websocket_session::send(std::string&& message) {
  queue_message(std::move(message));
}

void websocket_session::do_write() {
  std::string message;
  {
    std::lock_guard<std::mutex> lock(write_queue_mutex_);
    if (write_queue_.empty()) {
      writing_.store(false, std::memory_order_release);
      return;
    }

    writing_.store(true, std::memory_order_release);
    message = write_queue_.front();
  }

  // Write the message
  ws_.async_write(boost::asio::buffer(message),
                  boost::beast::bind_front_handler(&websocket_session::on_write,
                                                    shared_from_this()));
}

void websocket_session::on_write(boost::beast::error_code ec,
                                   std::size_t bytes_transferred) {
  boost::ignore_unused(bytes_transferred);

  if (ec) {
    cppsim::server::log_error(std::string("[WebSocketSession] Write error for ") + get_session_id_safe() + ": " + ec.message());
    writing_.store(false, std::memory_order_release);
    std::queue<std::string> empty;
    {
      std::lock_guard<std::mutex> lock(write_queue_mutex_);
      std::swap(write_queue_, empty);
    }
    do_close();
    return;
  }

  // Remove the sent message from queue
  {
    std::lock_guard<std::mutex> lock(write_queue_mutex_);
    write_queue_.pop();

    if (write_queue_.empty()) {
      writing_.store(false, std::memory_order_release);
      if (should_close_.load(std::memory_order_acquire)) {
        do_close();
      }
      return;
    }
  }

  // Continue writing if more messages queued
  do_write();
}

void websocket_session::check_deadline() {
  auto self = shared_from_this();

  deadline_.async_wait(
      [self](boost::beast::error_code ec) {
        if (ec == boost::asio::error::operation_aborted) {
           return;
        }

        if (ec) {
           if (self->state_.load(std::memory_order_acquire) == state::closed) {
              return;
           }

           cppsim::server::log_error(std::string("[WebSocketSession] Timer error: ") + ec.message());
           return;
        }

        auto current_state = self->state_.load(std::memory_order_acquire);

        if (current_state == state::closed) {
          return;
        }

        if (current_state == state::unauthenticated) {
            cppsim::server::log_error("[WebSocketSession] Handshake timeout");

            boost::beast::error_code socket_ec;
            self->ws_.next_layer().socket().close(socket_ec);
            self->state_.store(state::closed, std::memory_order_release);
         } else {
             cppsim::server::log_error(std::string("[WebSocketSession] Idle timeout for session ") + self->get_session_id_safe());
             self->close();
         }
  });
}

void websocket_session::close() {
  auto current_state = state_.load(std::memory_order_acquire);
  if (current_state == state::closed) {
    return;
  }

  boost::asio::post(ws_.get_executor(), [self = shared_from_this()]() {
     if (self->writing_.load(std::memory_order_acquire)) {
         self->should_close_.store(true, std::memory_order_release);
     } else {
         self->do_close();
     }
  });
}

std::string websocket_session::get_session_id_safe() const {
  std::lock_guard<std::mutex> lock(session_id_mutex_);
  return session_id_;
}

bool websocket_session::validate_session_id(const std::string& provided_session_id) {
  if (provided_session_id.empty()) {
    cppsim::server::log_error("[WebSocketSession] Empty session ID provided");
    protocol::error_message err;
    err.error_code = protocol::error_codes::PROTOCOL_ERROR;
    err.message = "Session ID is required";
    send(protocol::serialize_error(err));
    return false;
  }

  if (provided_session_id.length() > config::MAX_SESSION_ID_LENGTH) {
    cppsim::server::log_error(std::string("[WebSocketSession] Session ID too long: ") + 
               std::to_string(provided_session_id.length()) + " > " + 
               std::to_string(config::MAX_SESSION_ID_LENGTH));
    protocol::error_message err;
    err.error_code = protocol::error_codes::PROTOCOL_ERROR;
    err.message = "Session ID exceeds maximum length";
    send(protocol::serialize_error(err));
    return false;
  }

  std::string session_id_copy = get_session_id_safe();
  
  if (provided_session_id != session_id_copy) {
    cppsim::server::log_error(std::string("[WebSocketSession] Session ID mismatch: expected ") + session_id_copy + ", got " +
               provided_session_id);
    protocol::error_message err;
    err.error_code = protocol::error_codes::PROTOCOL_ERROR;
    err.message = "Session ID mismatch";
    send(protocol::serialize_error(err));
    return false;
  }
  return true;
}

void websocket_session::do_close() {
    state expected = state::authenticated;
    if (!state_.compare_exchange_strong(expected, state::closed,
                                         std::memory_order_acq_rel)) {
      if (state_.load(std::memory_order_acquire) == state::closed) {
        return;
      }
      expected = state::unauthenticated;
      if (!state_.compare_exchange_strong(expected, state::closed,
                                           std::memory_order_acq_rel)) {
        return;
      }
    }

    std::string session_id_copy = get_session_id_safe();

    if (!session_id_copy.empty()) {
      if (auto mgr = conn_mgr_.lock()) {
        mgr->unregister_session(session_id_copy);
      }
    }

    ws_.async_close(boost::beast::websocket::close_code::normal,
                    [self = shared_from_this()](boost::beast::error_code ec) {
                      if (ec) {
                        cppsim::server::log_error(std::string("[WebSocketSession] Close error: ") + ec.message());
                      }
                    });
}

}  // namespace server
}  // namespace cppsim
