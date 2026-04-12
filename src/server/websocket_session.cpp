#include "websocket_session.hpp"

#include <algorithm>

#include "connection_manager.hpp"
#include "logger.hpp"
#include "protocol.hpp"

namespace cppsim {
namespace server {

namespace {

// Truncate session ID for safe logging (show only first 8 chars of hex part)
std::string sanitize_sid(const std::string& sid) {
  if (sid.size() <= 13) return sid;
  return sid.substr(0, 13) + "...";
}

}  // namespace

websocket_session::websocket_session(
    boost::asio::ip::tcp::socket socket,
    std::shared_ptr<connection_manager> mgr)
    : ws_(std::move(socket)), conn_mgr_(mgr), deadline_(ws_.get_executor()) {}

websocket_session::~websocket_session() noexcept {
  try {
    boost::beast::error_code ec;
    deadline_.cancel(ec);
    if (state_.load(std::memory_order_acquire) != state::closed) {
      if (auto mgr = conn_mgr_.lock()) {
        std::string sid = get_session_id_safe();
        if (!sid.empty()) {
          mgr->unregister_session(std::move(sid));
        }
      }
    }
  } catch (...) {
  }
}

void websocket_session::run() {
  ws_.set_option(boost::beast::websocket::stream_base::timeout{
      std::chrono::hours(24),
      std::chrono::hours(24),
      false});

  ws_.read_message_max(config::MAX_MESSAGE_SIZE);

  deadline_.expires_after(config::HANDSHAKE_TIMEOUT);
  check_deadline();

  do_accept();
}

void websocket_session::do_accept() {
  ws_.async_accept(boost::beast::bind_front_handler(
      &websocket_session::on_accept, shared_from_this()));
}

void websocket_session::on_accept(boost::beast::error_code ec) {
  if (ec) {
    log_error(std::string("[WebSocketSession] Accept failed: ") + ec.message());
    boost::beast::error_code timer_ec;
    deadline_.cancel(timer_ec);
    state_.store(state::closed, std::memory_order_release);
    return;
  }

  log_message("[WebSocketSession] Connection accepted. Waiting for handshake...");

  do_read();
}

void websocket_session::do_read() {
  boost::asio::dispatch(ws_.get_executor(), [self = shared_from_this()]() {
    if (self->state_.load(std::memory_order_acquire) == state::closed) {
      return;
    }
    self->ws_.async_read(self->buffer_, boost::beast::bind_front_handler(
                                &websocket_session::on_read, self));
  });
}

void websocket_session::on_read(boost::beast::error_code ec,
                                 [[maybe_unused]] std::size_t bytes_transferred) {
  auto current_state = state_.load(std::memory_order_acquire);

  if (current_state == state::closed) {
    return;
  }

  if (ec == boost::beast::websocket::error::closed) {
    if (state_.exchange(state::closed, std::memory_order_acq_rel) == state::closed) {
      return;
    }
    std::string sid = get_session_id_safe();
    if (sid.empty()) {
      log_message("[WebSocketSession] Unauthenticated client disconnected");
    } else {
      log_message(std::string("[WebSocketSession] Client disconnected: ") + sanitize_sid(sid));
    }
    if (auto mgr = conn_mgr_.lock()) {
      mgr->unregister_session(std::move(sid));
    }
    return;
  }

  if (ec) {
    if (state_.exchange(state::closed, std::memory_order_acq_rel) == state::closed) {
      return;
    }
    std::string sid = get_session_id_safe();
    if (sid.empty()) {
      log_error(std::string("[WebSocketSession] Read error (unauthenticated): ") + ec.message());
    } else {
      log_error(std::string("[WebSocketSession] Read error for ") + sanitize_sid(sid) + ": " + ec.message());
    }
    if (auto mgr = conn_mgr_.lock()) {
      mgr->unregister_session(std::move(sid));
    }
    return;
  }

  std::string message = boost::beast::buffers_to_string(buffer_.data());
  buffer_.consume(buffer_.size());

  if (!check_rate_limit()) {
    return;
  }

  try {
    if (current_state == state::unauthenticated) {
      handle_handshake_message(message);
    } else {
      handle_authenticated_message(message);
    }
  } catch (const std::exception& e) {
    log_error(std::string("[WebSocketSession] Unhandled exception in message handler: ") + e.what());
    send_protocol_error(protocol::error_codes::PROTOCOL_ERROR, "Internal server error");
    close();
  } catch (...) {
    log_error("[WebSocketSession] Unknown exception in message handler");
    close();
  }

  if (state_.load(std::memory_order_acquire) != state::closed &&
      !close_requested_.load(std::memory_order_acquire)) {
    deadline_.expires_after(config::IDLE_TIMEOUT);
    do_read();
  }
}

bool websocket_session::check_rate_limit() {
  auto now = std::chrono::steady_clock::now();
  bool rate_exceeded = false;

  {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);
    auto window_start = now - config::RATE_LIMIT_WINDOW;
    auto it = std::remove_if(message_timestamps_.begin(), message_timestamps_.end(),
        [window_start](const auto& timestamp) { return timestamp < window_start; });
    message_timestamps_.erase(it, message_timestamps_.end());

    if (message_timestamps_.size() >= config::MAX_MESSAGES_PER_WINDOW) {
      rate_exceeded = true;
    } else {
      while (message_timestamps_.size() >= config::MAX_TIMESTAMPS_TO_TRACK) {
        message_timestamps_.pop_front();
      }
      message_timestamps_.push_back(now);
    }
  }

  if (rate_exceeded) {
    log_error("[WebSocketSession] Rate limit exceeded (max " +
        std::to_string(config::MAX_MESSAGES_PER_WINDOW) + " messages per window) for session " + sanitize_sid(get_session_id_safe()));
    send_protocol_error(protocol::error_codes::PROTOCOL_ERROR, "Rate limit exceeded");
    close();
    return false;
  }

  return true;
}

void websocket_session::handle_handshake_message(const std::string& message) {
  auto handshake_opt = protocol::parse_handshake(message);

  if (!handshake_opt) {
    log_error("[WebSocketSession] Handshake error: Protocol error (Not HANDSHAKE)");
    send_protocol_error(protocol::error_codes::MALFORMED_HANDSHAKE, "Expected HANDSHAKE message");
    close();
    return;
  }

  const auto& handshake_msg = *handshake_opt;

  if (handshake_msg.protocol_version != protocol::PROTOCOL_VERSION) {
    // Truncate version for logging to prevent log injection
    std::string safe_version = handshake_msg.protocol_version.substr(0, 32);
    log_error(std::string("[WebSocketSession] Handshake error: Incompatible version ") + safe_version);
    send_protocol_error(protocol::error_codes::INCOMPATIBLE_VERSION,
                        std::string("Expected ") + protocol::PROTOCOL_VERSION);
    close();
    return;
  }

  if (handshake_msg.client_name) {
    log_message(std::string("[WebSocketSession] Client Name: ") + handshake_msg.client_name->substr(0, 32));
  }

  deadline_.expires_after(config::IDLE_TIMEOUT);

  std::string new_session_id;
  if (auto mgr = conn_mgr_.lock()) {
    new_session_id = mgr->register_session(shared_from_this());
    if (new_session_id.empty()) {
      log_error("[WebSocketSession] Failed to register session - ID collision");
      send_protocol_error(protocol::error_codes::PROTOCOL_ERROR, "Failed to generate unique session ID");
      close();
      return;
    }
  } else {
    log_error("[WebSocketSession] Warning: No connection manager, session ID invalid");
    send_protocol_error(protocol::error_codes::PROTOCOL_ERROR, "Connection manager not available");
    close();
    return;
  }

  {
    std::lock_guard<std::mutex> lock(session_id_mutex_);
    session_id_ = new_session_id;
  }

  protocol::handshake_response resp;
  resp.session_id = new_session_id;
  resp.seat_number = config::PLACEHOLDER_SEAT;
  resp.starting_stack = config::PLACEHOLDER_STACK;

  if (!send(protocol::serialize_handshake_response(resp))) {
    log_error("[WebSocketSession] Failed to send handshake response for session: " + new_session_id);
    close();
    return;
  }

  state_.store(state::authenticated, std::memory_order_release);

  log_message(std::string("[WebSocketSession] Handshake successful for session: ") + sanitize_sid(new_session_id));
}

void websocket_session::handle_authenticated_message(const std::string& message) {
  auto header_opt = protocol::extract_message_type_and_json(message);

  if (!header_opt) {
    log_message(std::string("[WebSocketSession] Invalid message format from ") + get_session_id_safe() + ": missing message_type");
    send_protocol_error(protocol::error_codes::MALFORMED_MESSAGE, "Missing or invalid message_type field");
    close();
    return;
  }

  const auto& msg_type = header_opt->message_type;
  std::string sid = get_session_id_safe();

  if (msg_type == protocol::message_types::ACTION) {
    handle_action(message, sid);
  } else if (msg_type == protocol::message_types::RELOAD_REQUEST) {
    handle_reload_msg(message, sid);
  } else if (msg_type == protocol::message_types::DISCONNECT) {
    handle_disconnect_msg(message, sid);
  } else {
    log_message(std::string("[WebSocketSession] Unknown message type '") + msg_type + "' from " + sid);
    send_protocol_error(protocol::error_codes::PROTOCOL_ERROR,
                        std::string("Unknown message type: ") + msg_type);
    close();
  }
}

void websocket_session::handle_action(const std::string& message, const std::string& sid) {
  auto action_opt = protocol::parse_action(message);
  if (!action_opt) {
    log_error("[WebSocketSession] Failed to parse ACTION message from " + sid);
    send_protocol_error(protocol::error_codes::MALFORMED_MESSAGE, "Invalid ACTION message format");
    return;
  }

  if (!validate_session_id(action_opt->session_id)) {
    return;
  }

  int64_t seq = action_opt->sequence_number;
  int64_t last_seq = last_sequence_number_.load(std::memory_order_acquire);
  if (seq <= last_seq) {
    log_error("[WebSocketSession] Invalid sequence number " +
              std::to_string(seq) + " (expected > " +
              std::to_string(last_seq) + ")");
    send_protocol_error(protocol::error_codes::PROTOCOL_ERROR, "Invalid sequence number - possible replay attack");
    close();
    return;
  }
  uint64_t gap = static_cast<uint64_t>(seq) - static_cast<uint64_t>(last_seq);
  if (gap > static_cast<uint64_t>(config::MAX_SEQUENCE_GAP)) {
    log_error("[WebSocketSession] Sequence number too far ahead: " +
              std::to_string(seq) + " (gap: " + std::to_string(gap) +
              ", max allowed: " + std::to_string(config::MAX_SEQUENCE_GAP) + ")");
    send_protocol_error(protocol::error_codes::PROTOCOL_ERROR, "Sequence number gap too large");
    close();
    return;
  }
  last_sequence_number_.store(seq, std::memory_order_release);
  log_message(std::string("[WebSocketSession] Validated ACTION from ") + sid + ": type=" +
              action_opt->action_type + " seq=" + std::to_string(seq));
}

void websocket_session::handle_reload_msg(const std::string& message, const std::string& sid) {
  auto reload_opt = protocol::parse_reload_request(message);
  if (!reload_opt) {
    log_error("[WebSocketSession] Failed to parse RELOAD_REQUEST from " + sid);
    send_protocol_error(protocol::error_codes::MALFORMED_MESSAGE, "Invalid RELOAD_REQUEST format");
    return;
  }
  if (validate_session_id(reload_opt->session_id)) {
    log_message(std::string("[WebSocketSession] Validated RELOAD_REQUEST from ") + sid);
    protocol::reload_response_message resp;
    resp.granted = true;
    current_stack_ = std::min(current_stack_ + reload_opt->requested_amount, protocol::MAX_AMOUNT);
    resp.new_stack = current_stack_;
    if (!send(protocol::serialize_reload_response(resp))) {
      log_error("[WebSocketSession] Failed to send RELOAD_RESPONSE to " + sid);
    }
  }
}

void websocket_session::handle_disconnect_msg(const std::string& message, const std::string& sid) {
  auto disconnect_opt = protocol::parse_disconnect(message);
  if (!disconnect_opt) {
    log_error("[WebSocketSession] Failed to parse DISCONNECT from " + sid);
    send_protocol_error(protocol::error_codes::MALFORMED_MESSAGE, "Invalid DISCONNECT format");
    return;
  }
  if (validate_session_id(disconnect_opt->session_id)) {
    log_message(std::string("[WebSocketSession] Validated DISCONNECT from ") + sid);
    close();
  }
}

bool websocket_session::queue_message(std::string&& message) {
  bool should_post = false;
  {
    std::lock_guard<std::mutex> lock(write_queue_mutex_);
    if (state_.load(std::memory_order_acquire) == state::closed ||
        close_requested_.load(std::memory_order_acquire)) {
      return false;
    }
    if (write_queue_.size() >= config::MAX_WRITE_QUEUE_SIZE) {
      log_error(std::string("[WebSocketSession] Write queue full for session ") + sanitize_sid(get_session_id_safe()) + ", dropping message");
      return false;
    }
    write_queue_.push(std::move(message));
    should_post = !writing_;
    if (should_post) writing_ = true;
  }

  if (should_post) {
    boost::asio::post(ws_.get_executor(),
                      [self = shared_from_this()]() {
                         self->do_write();
                      });
  }
  return true;
}

bool websocket_session::send(std::string message) {
  return queue_message(std::move(message));
}

void websocket_session::do_write() {
  std::shared_ptr<std::string> message;
  {
    std::lock_guard<std::mutex> lock(write_queue_mutex_);
    if (state_.load(std::memory_order_acquire) == state::closed) {
      writing_ = false;
      return;
    }
    if (write_queue_.empty()) {
      writing_ = false;
      return;
    }

    writing_ = true;
    message = std::make_shared<std::string>(std::move(write_queue_.front()));
    write_queue_.pop();
  }

  ws_.async_write(boost::asio::buffer(*message),
                  [self = shared_from_this(), message](boost::beast::error_code ec, std::size_t bytes) {
                    self->on_write(ec, bytes);
                  });
}

void websocket_session::on_write(boost::beast::error_code ec,
                                   [[maybe_unused]] std::size_t bytes_transferred) {
  if (ec) {
    log_error(std::string("[WebSocketSession] Write error for ") + sanitize_sid(get_session_id_safe()) + ": " + ec.message());
    size_t dropped = 0;
    {
      std::lock_guard<std::mutex> lock(write_queue_mutex_);
      writing_ = false;
      close_requested_.store(true, std::memory_order_release);
      dropped = write_queue_.size();
      write_queue_ = std::queue<std::string>();
    }
    if (dropped > 0) {
      log_error("[WebSocketSession] Dropped " + std::to_string(dropped) + " queued message(s) on write error");
    }
    do_close();
    return;
  }

  bool should_close = false;
  bool has_more = false;
  {
    std::lock_guard<std::mutex> lock(write_queue_mutex_);
    writing_ = false;
    if (close_requested_.load(std::memory_order_acquire) && write_queue_.empty()) {
      should_close = true;
    } else if (!write_queue_.empty()) {
      writing_ = true;
      has_more = true;
    }
  }

  if (should_close) {
    do_close();
    return;
  }

  if (has_more) {
    boost::asio::post(ws_.get_executor(), [self = shared_from_this()]() {
      self->do_write();
    });
  }
}

void websocket_session::check_deadline() {
  deadline_.async_wait(
      [self = shared_from_this()](boost::beast::error_code ec) {
        if (ec == boost::asio::error::operation_aborted) {
          if (self->state_.load(std::memory_order_acquire) != state::closed &&
              !self->close_requested_.load(std::memory_order_acquire)) {
            self->check_deadline();
          }
          return;
        }

        if (ec) {
          if (self->state_.load(std::memory_order_acquire) == state::closed) {
            return;
          }

          log_error(std::string("[WebSocketSession] Timer error: ") + ec.message());
          return;
        }

        auto current_state = self->state_.load(std::memory_order_acquire);

        if (current_state == state::closed) {
          return;
        }

        if (current_state == state::unauthenticated) {
          log_error("[WebSocketSession] Handshake timeout");
          self->close();
        } else {
          log_error(std::string("[WebSocketSession] Idle timeout for session ") + sanitize_sid(self->get_session_id_safe()));
          self->close();
        }
      });
}

void websocket_session::close() noexcept {
  close_requested_.store(true, std::memory_order_release);

  auto current_state = state_.load(std::memory_order_acquire);
  if (current_state == state::closed) {
    return;
  }

  try {
    boost::asio::post(ws_.get_executor(), [self = shared_from_this()]() {
       bool write_in_flight = false;
       {
         std::lock_guard<std::mutex> lock(self->write_queue_mutex_);
         write_in_flight = self->writing_;
       }
       if (!write_in_flight) {
         self->do_close();
       }
    });
  } catch (...) {
    log_error("[WebSocketSession] Exception in close() - forcing state to closed");
    state_.store(state::closed, std::memory_order_release);
    boost::beast::error_code timer_ec;
    deadline_.cancel(timer_ec);
  }
}

std::string websocket_session::get_session_id_safe() const noexcept {
  try {
    std::lock_guard<std::mutex> lock(session_id_mutex_);
    return session_id_;
  } catch (...) {
    return "";
  }
}

bool websocket_session::validate_session_id(const std::string& provided_session_id) noexcept {
  try {
    if (provided_session_id.empty()) {
      log_error("[WebSocketSession] Empty session ID provided");
      send_protocol_error(protocol::error_codes::PROTOCOL_ERROR, "Session ID is required");
      return false;
    }

    if (provided_session_id.length() > config::MAX_SESSION_ID_LENGTH) {
      log_error(std::string("[WebSocketSession] Session ID too long: ") +
                  std::to_string(provided_session_id.length()) + " > " +
                  std::to_string(config::MAX_SESSION_ID_LENGTH));
      send_protocol_error(protocol::error_codes::PROTOCOL_ERROR, "Session ID exceeds maximum length");
      return false;
    }

    std::string session_id_copy = get_session_id_safe();

    if (provided_session_id != session_id_copy) {
      log_error(std::string("[WebSocketSession] Session ID mismatch: expected ") + sanitize_sid(session_id_copy) + ", got " +
                  sanitize_sid(provided_session_id));
      send_protocol_error(protocol::error_codes::PROTOCOL_ERROR, "Session ID mismatch");
      close();
      return false;
    }
    return true;
  } catch (...) {
    log_error("[WebSocketSession] Exception in validate_session_id");
    return false;
  }
}

void websocket_session::send_protocol_error(const char* error_code, std::string_view message) noexcept {
  try {
    protocol::error_message err;
    err.error_code = error_code;
    err.message = std::string(message);
    if (!send(protocol::serialize_error(err))) {
      log_error("[WebSocketSession] Failed to send protocol error for session " + sanitize_sid(get_session_id_safe()));
    }
  } catch (...) {
    log_error("[WebSocketSession] Exception in send_protocol_error");
  }
}

void websocket_session::do_close() noexcept {
  // Use close_initiated_ to prevent double async_close even on same strand
  bool expected = false;
  if (!close_initiated_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
    return;
  }

  if (state_.exchange(state::closed, std::memory_order_acq_rel) == state::closed) {
    return;
  }

  try {
    boost::beast::error_code timer_ec;
    deadline_.cancel(timer_ec);

    std::string session_id_copy = get_session_id_safe();

    if (!session_id_copy.empty()) {
      if (auto mgr = conn_mgr_.lock()) {
        mgr->unregister_session(std::move(session_id_copy));
      }
    }

    ws_.async_close(boost::beast::websocket::close_code::normal,
                    [self = shared_from_this()](boost::beast::error_code ec) {
                      if (ec) {
                        log_error(std::string("[WebSocketSession] Close error: ") + ec.message());
                        // Fallback: force-close the TCP socket to prevent FD leak
                        try {
                          self->ws_.next_layer().close();
                        } catch (...) {
                        }
                      }
                    });
  } catch (const std::exception& e) {
    log_error(std::string("[WebSocketSession] Exception in do_close: ") + e.what());
  }
}

}  // namespace server
}  // namespace cppsim
