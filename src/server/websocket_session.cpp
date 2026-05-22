#include "websocket_session.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <utility>

#include "connection_manager.hpp"
#include "logger.hpp"
#include "protocol.hpp"
#include "sanitize.hpp"
#include "runtime_config.hpp"
#include "string_utils.hpp"

namespace cppsim {
namespace server {

websocket_session::websocket_session(
    boost::asio::ip::tcp::socket socket,
    std::shared_ptr<connection_manager> mgr,
    std::chrono::seconds handshake_timeout)
    : ws_(std::move(socket)),
      conn_mgr_(mgr),
      deadline_(ws_.get_executor()),
      handshake_timeout_(handshake_timeout) {}

websocket_session::~websocket_session() noexcept {
  boost::beast::error_code ec;
  deadline_.cancel(ec);
  
  if (state_.load(std::memory_order_acquire) != state::closed) {
    if (auto mgr = conn_mgr_.lock()) {
      std::string sid = get_session_id_safe();
      if (!sid.empty()) {
        mgr->unregister_session(sid);
      }
    }
  }
}

void websocket_session::run() noexcept {
  try {
    ws_.set_option(boost::beast::websocket::stream_base::timeout{
        config::WS_IDLE_TIMEOUT,
        config::WS_READ_TIMEOUT,
        false});

    ws_.read_message_max(enhanced_config::get_max_message_size());

    deadline_.expires_after(handshake_timeout_);
    check_deadline();

    do_accept();
  } catch (const std::exception& e) {
    log_error(std::string("[WebSocketSession] run() initialization error: ") + e.what());
    state_.store(state::closed, std::memory_order_release);
    boost::beast::error_code ec;
    deadline_.cancel(ec);
  } catch (...) {
    log_error("[WebSocketSession] run() unknown initialization error");
    state_.store(state::closed, std::memory_order_release);
    boost::beast::error_code ec;
    deadline_.cancel(ec);
  }
}

void websocket_session::do_accept() {
  ws_.async_accept(boost::beast::bind_front_handler(
      &websocket_session::on_accept, shared_from_this()));
}

void websocket_session::on_accept(boost::beast::error_code ec) {
  if (ec) {
    // If the session was already closed (e.g. handshake timeout fired while
    // async_accept was pending), do_close() already handled cleanup.
    if (state_.load(std::memory_order_acquire) == state::closed) {
      return;
    }
    // close() may have been called but do_close() hasn't run yet on the
    // strand — suppress the log to avoid noise during graceful shutdown.
    if (close_requested_.load(std::memory_order_acquire)) {
      boost::beast::error_code timer_ec;
      deadline_.cancel(timer_ec);
      state_.store(state::closed, std::memory_order_release);
      return;
    }
    // Probe connections (e.g. health checks) close before completing the WS
    // handshake — this is normal, not a real error.  Clean up timer and state
    // to avoid a spurious "Handshake timeout" log when the deadline fires.
    if (ec != boost::beast::websocket::error::closed) {
      try {
        log_error(std::string("[WebSocketSession] Accept failed: ") + ec.message());
      } catch (...) {
        // Allocation failure in async handler — log is best-effort.
      }
    }
    boost::beast::error_code timer_ec;
    deadline_.cancel(timer_ec);
    state_.store(state::closed, std::memory_order_release);
    return;
  }

  // Guard: session may have been closed (e.g. handshake timeout) while
  // async_accept was in flight.
  if (state_.load(std::memory_order_acquire) == state::closed ||
      close_requested_.load(std::memory_order_acquire)) {
    return;
  }

  try {
    log_message("[WebSocketSession] Connection accepted. Waiting for handshake...");
    do_read();
  } catch (const std::exception& e) {
    log_error(std::string("[WebSocketSession] Exception in on_accept: ") + e.what());
    close();
  } catch (...) {
    log_error("[WebSocketSession] Unknown exception in on_accept");
    close();
  }
}

void websocket_session::do_read() {
  boost::asio::dispatch(ws_.get_executor(), [self = shared_from_this()]() {
    if (self->state_.load(std::memory_order_acquire) == state::closed ||
        self->close_requested_.load(std::memory_order_acquire)) {
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
    boost::beast::error_code timer_ec;
    deadline_.cancel(timer_ec);
    try {
      std::string sid = get_session_id_safe();
      if (sid.empty()) {
        log_message("[WebSocketSession] Unauthenticated client disconnected");
      } else {
        log_message(std::string("[WebSocketSession] Client disconnected: ") + sanitize_session_id(sid));
        if (auto mgr = conn_mgr_.lock()) {
          mgr->unregister_session(sid);
        }
      }
    } catch (...) {
      // Allocation failure in async handler — session state was already
      // transitioned to closed and the timer cancelled.  unregister_session
      // will happen when stop_all() runs or the session is destroyed.
    }
    return;
  }

  if (ec) {
    if (state_.exchange(state::closed, std::memory_order_acq_rel) == state::closed) {
      return;
    }
    boost::beast::error_code timer_ec;
    deadline_.cancel(timer_ec);
    try {
      std::string sid = get_session_id_safe();
      if (sid.empty()) {
        log_error(std::string("[WebSocketSession] Read error (unauthenticated): ") + ec.message());
      } else {
        log_error(std::string("[WebSocketSession] Read error for ") + sanitize_session_id(sid) + ": " + ec.message());
        if (auto mgr = conn_mgr_.lock()) {
          mgr->unregister_session(sid);
        }
      }
    } catch (...) {
      // Allocation failure in async handler — same reasoning as above.
    }
    return;
  }

  try {
    std::string message = boost::beast::buffers_to_string(buffer_.data());
    buffer_.consume(buffer_.size());

    if (!check_rate_limit_or_close()) {
      return;
    }

    if (current_state == state::unauthenticated) {
      handle_handshake_message(message);
    } else {
      handle_authenticated_message(message);
    }
  } catch (const std::exception& e) {
    try {
      log_error(std::string("[WebSocketSession] Unhandled exception in message handler: ") + e.what());
    } catch (...) {
    }
    send_protocol_error(protocol::error_codes::PROTOCOL_ERROR, "Internal server error");
    close();
    // Fall through is safe: close() synchronously sets close_requested_ before
    // dispatching do_close() to the strand, so the post-try-catch guard below
    // will prevent do_read() from being scheduled.
    return;
  } catch (...) {
    try {
      log_error("[WebSocketSession] Unknown exception in message handler for session " +
                sanitize_session_id(get_session_id_safe()));
    } catch (...) {
    }
    send_protocol_error(protocol::error_codes::PROTOCOL_ERROR, "Internal server error");
    close();
    // See above: close() sets close_requested_ synchronously — safe to fall through.
    return;
  }

  // Guard: only schedule the next read if the session is still alive.
  // close() sets close_requested_ synchronously (before strand dispatch),
  // and do_close() sets state_=closed under a CAS.  Either flag prevents
  // do_read() from being scheduled, avoiding a use-after-close read.
  if (state_.load(std::memory_order_acquire) != state::closed &&
      !close_requested_.load(std::memory_order_acquire)) {
    deadline_.expires_after(enhanced_config::get_idle_timeout());
    do_read();
  }
}

bool websocket_session::check_rate_limit_or_close() noexcept {
  auto now = std::chrono::steady_clock::now();
  bool should_close = false;

  {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);
    auto window_start = now - enhanced_config::get_rate_limit_window();
    auto it = std::remove_if(message_timestamps_.begin(), message_timestamps_.end(),
        [window_start](const auto& timestamp) { return timestamp < window_start; });
    message_timestamps_.erase(it, message_timestamps_.end());

    if (message_timestamps_.size() >= enhanced_config::get_max_messages_per_window()) {
      should_close = true;
    } else {
      message_timestamps_.push_back(now);
    }
  }

  if (should_close) {
    // Read session_id outside the rate_limit_mutex_ to avoid nested lock
    // acquisition (rate_limit_mutex_ -> session_id_mutex_).
    // String construction is wrapped in try/catch because this function is
    // noexcept — an allocation failure in concatenation would call std::terminate.
    try {
      std::string session_id_str = get_session_id_safe();
      std::string id_str = session_id_str.empty() ? "(unauthenticated)" : sanitize_session_id(session_id_str);
      log_error("[WebSocketSession] Rate limit exceeded (max " +
          std::to_string(config::MAX_MESSAGES_PER_WINDOW) + " messages per window) for session " + id_str);
    } catch (...) {
      log_error("[WebSocketSession] Rate limit exceeded");
    }
    send_protocol_error(protocol::error_codes::SESSION_CLOSED, "Rate limit exceeded");
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
    log_error(std::string("[WebSocketSession] Handshake error: Incompatible version ") + trunc_field(handshake_msg.protocol_version, 32));
    send_protocol_error(protocol::error_codes::INCOMPATIBLE_VERSION,
                        std::string("Expected ") + protocol::PROTOCOL_VERSION);
    close();
    return;
  }

  if (handshake_msg.client_name) {
    const auto& cname = *handshake_msg.client_name;
    try {
      log_message(std::string("[WebSocketSession] Client Name: ") + trunc_field(cname, 32));
    } catch (...) {
      // Non-fatal: log allocation failure must not unwind a successful handshake.
    }
  }

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

  try {
    log_message(std::string("[WebSocketSession] Handshake successful for session: ") + sanitize_session_id(new_session_id));
  } catch (...) {
    // Non-fatal: handshake already succeeded, session is registered and response queued.
  }
}

void websocket_session::handle_authenticated_message(const std::string& message) {
  auto header_opt = protocol::extract_message_type_and_json(message);

  if (!header_opt) {
    log_error(std::string("[WebSocketSession] Invalid message format from ") + sanitize_session_id(get_session_id_safe()) + ": missing message_type");
    send_protocol_error(protocol::error_codes::MALFORMED_MESSAGE, "Missing or invalid message_type field");
    close();
    return;
  }

  const auto& msg_type = header_opt->message_type;
  const std::string sid = get_session_id_safe();

  if (msg_type == protocol::message_types::ACTION) {
    handle_action(*header_opt, sid);
  } else if (msg_type == protocol::message_types::RELOAD_REQUEST) {
    handle_reload_msg(*header_opt, sid);
  } else if (msg_type == protocol::message_types::DISCONNECT) {
    handle_disconnect_msg(*header_opt, sid);
  } else {
    try {
      log_error(std::string("[WebSocketSession] Unknown message type '") + trunc_field(msg_type) + "' from " + sanitize_session_id(sid));
    } catch (...) {
      // Allocation failure in log — session will still be closed below.
    }
    try {
      send_protocol_error(protocol::error_codes::PROTOCOL_ERROR,
                          std::string("Unknown message type: ") + trunc_field(msg_type));
    } catch (...) {
      // Allocation failure — close without sending the specific error.
      // on_read's catch block won't help here because we're not throwing.
    }
    close();
  }
}

void websocket_session::handle_action(const protocol::parsed_message_header& header, const std::string& sid) {
  auto action_opt = protocol::parse_action_from_envelope(header.envelope_json);
  if (!action_opt) {
    log_error("[WebSocketSession] Failed to parse ACTION message from " + sanitize_session_id(sid));
    send_protocol_error(protocol::error_codes::MALFORMED_MESSAGE, "Invalid ACTION message format");
    close();
    return;
  }

  if (!validate_session_id(action_opt->session_id)) {
    return;
  }

  // Basic action validation
  if (action_opt->action_type.empty()) {
    send_protocol_error(protocol::error_codes::PROTOCOL_ERROR, "Action type cannot be empty");
    close();
    return;
  }

  // Validate action-specific requirements
  if (action_opt->action_type == protocol::action_types::RAISE || 
      action_opt->action_type == protocol::action_types::ALL_IN) {
    if (!action_opt->amount || *action_opt->amount <= 0) {
      send_protocol_error(protocol::error_codes::PROTOCOL_ERROR, "RAISE and ALL_IN require a positive amount");
      close();
      return;
    }
    
    // Validate amount against maximum
    if (*action_opt->amount > protocol::MAX_AMOUNT) {
      send_protocol_error(protocol::error_codes::PROTOCOL_ERROR, "Amount exceeds maximum allowed");
      close();
      return;
    }
  } else if (action_opt->amount) {
    // Non-raise actions should not have an amount field
    send_protocol_error(protocol::error_codes::PROTOCOL_ERROR, 
                       (action_opt->action_type == protocol::action_types::FOLD ||
                        action_opt->action_type == protocol::action_types::CHECK ||
                        action_opt->action_type == protocol::action_types::CALL) 
                       ? "Action type does not accept amount" : "Invalid amount for action type");
    close();
    return;
  }

  int64_t seq = action_opt->sequence_number;
  int64_t last_seq = last_sequence_number_.load(std::memory_order_acquire);
  if (seq <= last_seq) {
    try {
      log_error("[WebSocketSession] Invalid sequence number " +
                std::to_string(seq) + " (expected > " +
                std::to_string(last_seq) + ")");
    } catch (...) {
      // Allocation failure in log — session will still be closed below.
    }
    send_protocol_error(protocol::error_codes::PROTOCOL_ERROR, "Invalid sequence number - possible replay attack");
    close();
    return;
  }
  // When last_seq is -1 (initial sentinel), casting to uint64_t wraps to UINT64_MAX,
  // so the subtraction yields seq + 1 — the correct gap from "no prior sequence".
  uint64_t gap = static_cast<uint64_t>(seq) - static_cast<uint64_t>(last_seq);
  if (gap > static_cast<uint64_t>(enhanced_config::get_max_sequence_gap())) {
    try {
      log_error("[WebSocketSession] Sequence number too far ahead: " +
                std::to_string(seq) + " (gap: " + std::to_string(gap) +
                ", max allowed: " + std::to_string(enhanced_config::get_max_sequence_gap()) + ")");
    } catch (...) {
      // Allocation failure in log — session will still be closed below.
    }
    send_protocol_error(protocol::error_codes::PROTOCOL_ERROR, "Sequence number gap too large");
    close();
    return;
  }
  last_sequence_number_.store(seq, std::memory_order_release);
  try {
    log_message(std::string("[WebSocketSession] Validated ACTION from ") + sanitize_session_id(sid) + ": type=" +
                action_opt->action_type + " seq=" + std::to_string(seq));
  } catch (...) {
    // Non-fatal: log allocation failure must not close a healthy session.
  }

  // TODO: Add game state validation once game engine is implemented:
  // - Verify current game phase allows this action type
  // - Check if it's the player's turn to act
  // - Validate stack amounts (RAISE/ALL_IN cannot exceed player stack)
  // - Check action legality against current betting round (e.g., cannot CHECK if bet made)
  // - Validate pot and bet amounts are within expected ranges
}

void websocket_session::handle_reload_msg(const protocol::parsed_message_header& header, const std::string& sid) {
  auto reload_opt = protocol::parse_reload_from_envelope(header.envelope_json);
  if (!reload_opt) {
    log_error("[WebSocketSession] Failed to parse RELOAD_REQUEST from " + sanitize_session_id(sid));
    send_protocol_error(protocol::error_codes::MALFORMED_MESSAGE, "Invalid RELOAD_REQUEST format");
    close();
    return;
  }
  if (!validate_session_id(reload_opt->session_id)) {
    return;
  }

  try {
    log_message(std::string("[WebSocketSession] Validated RELOAD_REQUEST from ") + sanitize_session_id(sid));
  } catch (...) {
    // Non-fatal: log allocation failure must not close a healthy session.
  }

  // Compute the new stack but only commit after the response is queued.
  // Safe without atomics: current_stack_ is only accessed from the session's
  // strand (single-threaded), so the read-then-write is not a data race.
  int64_t new_stack = std::min(current_stack_ + reload_opt->requested_amount, protocol::MAX_AMOUNT);
  protocol::reload_response_message resp;
  resp.granted = true;
  resp.new_stack = new_stack;
  if (!send(protocol::serialize_reload_response(resp))) {
    log_error("[WebSocketSession] Failed to send RELOAD_RESPONSE to " + sanitize_session_id(sid));
    close();
  } else {
    // Only commit the stack update after the response is successfully queued.
    // If the write ultimately fails in do_write, the stack will be inconsistent
    // with the client's view.  This is acceptable because a write failure causes
    // the session to close anyway — the inconsistency is not observable.
    current_stack_ = new_stack;
  }
}

void websocket_session::handle_disconnect_msg(const protocol::parsed_message_header& header, const std::string& sid) {
  auto disconnect_opt = protocol::parse_disconnect_from_envelope(header.envelope_json);
  if (!disconnect_opt) {
    log_error("[WebSocketSession] Failed to parse DISCONNECT from " + sanitize_session_id(sid));
    send_protocol_error(protocol::error_codes::MALFORMED_MESSAGE, "Invalid DISCONNECT format");
    close();
    return;
  }
  if (!validate_session_id(disconnect_opt->session_id)) {
    return;
  }

  try {
    log_message(std::string("[WebSocketSession] Validated DISCONNECT from ") + sanitize_session_id(sid));
  } catch (...) {
    // Non-fatal: log allocation failure must not prevent clean disconnect.
  }
  close();
}

bool websocket_session::queue_message(std::string&& message) noexcept {
  bool should_post = false;
  bool queue_full = false;
  {
    std::lock_guard<std::mutex> lock(write_queue_mutex_);
    if (state_.load(std::memory_order_acquire) == state::closed ||
        close_requested_.load(std::memory_order_acquire)) {
      return false;
    }
    if (write_queue_.size() >= config::MAX_WRITE_QUEUE_SIZE) {
      queue_full = true;
    } else {
      write_queue_.push(std::move(message));
      should_post = !writing_;
      if (should_post) writing_ = true;
    }
  }

  if (queue_full) {
    // Log outside the lock to reduce contention.  Wrapped in try/catch because
    // this function is noexcept — string concatenation failure would call
    // std::terminate.
    try {
      log_error("[WebSocketSession] Write queue full for session " +
                sanitize_session_id(get_session_id_safe()) + ", dropping message");
    } catch (...) {
      // Allocation failure — message was dropped regardless.
    }
    return false;
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
  // Pop the message string under the lock, then allocate the shared_ptr
  // outside the lock.  This reduces write_queue_mutex_ hold time and avoids
  // leaving a moved-from element in the queue if make_shared throws (the old
  // code moved from front() then called make_shared — if allocation failed,
  // front() was already empty but pop() hadn't executed).
  std::string msg_str;
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
    msg_str = std::move(write_queue_.front());
    write_queue_.pop();
  }

  std::shared_ptr<std::string> message;
  try {
    message = std::make_shared<std::string>(std::move(msg_str));
  } catch (...) {
    // Allocation failed — the dequeued message is lost.  Rather than
    // continuing with a broken write pipeline (client stuck waiting for a
    // response that will never arrive), close the session so the client
    // reconnects into a clean state.
    //
    // The log message construction is wrapped in a nested try/catch because
    // we are already in an allocation-failure path — the string concatenation
    // below could also fail with bad_alloc, and an uncaught exception from
    // this strand-posted handler would propagate through io_context::run(),
    // potentially crashing the entire server.
    try {
      log_error("[WebSocketSession] Exception in do_write (allocation failure) - closing session " +
                sanitize_session_id(get_session_id_safe()));
    } catch (...) {
      // Double allocation failure — nothing useful to log.
    }
    {
      std::lock_guard<std::mutex> lock(write_queue_mutex_);
      writing_ = false;
    }
    close();
    return;
  }

  ws_.async_write(boost::asio::buffer(*message),
                  [self = shared_from_this(), message](boost::beast::error_code ec, std::size_t bytes) {
                    self->on_write(ec, bytes);
                  });
}

void websocket_session::on_write(boost::beast::error_code ec,
                                   [[maybe_unused]] std::size_t bytes_transferred) {
  try {
    if (ec) {
      // String construction for log messages happens before the noexcept
      // log_error() is entered — wrap in try/catch so a bad_alloc from
      // concatenation doesn't propagate through io_context::run().
      try {
        log_error(std::string("[WebSocketSession] Write error for ") + sanitize_session_id(get_session_id_safe()) + ": " + ec.message());
      } catch (...) {
        // Allocation failure — best-effort fallback
        log_error("[WebSocketSession] Write error (allocation failure constructing log message)");
      }
      size_t dropped = 0;
      {
        std::lock_guard<std::mutex> lock(write_queue_mutex_);
        writing_ = false;
        close_requested_.store(true, std::memory_order_release);
        dropped = write_queue_.size();
        write_queue_ = std::queue<std::string>();
      }
      if (dropped > 0) {
        try {
          log_error("[WebSocketSession] Dropped " + std::to_string(dropped) + " queued message(s) on write error");
        } catch (...) {
          // Allocation failure — queue was still cleared successfully.
        }
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
  } catch (const std::exception& e) {
    try {
      log_error(std::string("[WebSocketSession] Exception in on_write: ") + e.what());
    } catch (...) {
    }
    do_close();
  } catch (...) {
    log_error("[WebSocketSession] Unknown exception in on_write");
    do_close();
  }
}

void websocket_session::check_deadline() {
  deadline_.async_wait(
      [self = shared_from_this()](boost::beast::error_code ec) {
        if (ec == boost::asio::error::operation_aborted) {
          // Only reschedule if the session is still alive and no close has been
          // requested.  When do_close() or close() cancels the timer we want to
          // stop the timer chain, not reschedule.
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

          try {
            log_error(std::string("[WebSocketSession] Timer error: ") + ec.message());
          } catch (...) {
          }
          // Reschedule the deadline check on transient timer errors so the
          // session isn't left without timeout protection.  Without this, any
          // non-aborted timer error would permanently kill the timer chain,
          // leaving only Beast's 24-hour stream timeout as a backstop.
          if (!self->close_requested_.load(std::memory_order_acquire)) {
            self->check_deadline();
          }
          return;
        }

        auto current_state = self->state_.load(std::memory_order_acquire);

        if (current_state == state::closed) {
          return;
        }

        if (current_state == state::unauthenticated) {
          log_error("[WebSocketSession] Handshake timeout");
          self->send_protocol_error(protocol::error_codes::SESSION_CLOSED, "Handshake timeout");
          self->close();
        } else {
          try {
            log_error(std::string("[WebSocketSession] Idle timeout for session ") + sanitize_session_id(self->get_session_id_safe()));
          } catch (...) {
            // Allocation failure in async handler — log is best-effort.
            log_error("[WebSocketSession] Idle timeout");
          }
          self->send_protocol_error(protocol::error_codes::SESSION_CLOSED, "Idle timeout");
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
    auto weak_self = weak_from_this();
    boost::asio::dispatch(ws_.get_executor(), [weak_self]() {
       auto self = weak_self.lock();
       if (!self) return;  // Object being destroyed — destructor handles cleanup
       bool write_in_flight = false;
       {
         std::lock_guard<std::mutex> lock(self->write_queue_mutex_);
         write_in_flight = self->writing_;
       }
       if (!write_in_flight) {
         self->do_close();
       }
    });
  } catch (const std::exception& e) {
    try {
      log_error(std::string("[WebSocketSession] Exception in close(): ") + e.what());
    } catch (...) {
      // Allocation failure in noexcept context — nothing useful to do.
    }
  } catch (...) {
    log_error("[WebSocketSession] Unknown exception in close()");
  }
}

std::string websocket_session::get_session_id_safe() const noexcept {
  try {
    std::lock_guard<std::mutex> lock(session_id_mutex_);
    return session_id_;
  } catch (...) {
    return std::string();
  }
}

bool websocket_session::validate_session_id(const std::string& provided_session_id) noexcept {
  try {
    if (provided_session_id.empty()) {
      log_error("[WebSocketSession] Empty session ID provided");
      send_protocol_error(protocol::error_codes::PROTOCOL_ERROR, "Session ID is required");
      close();
      return false;
    }

    if (provided_session_id.size() > config::MAX_SESSION_ID_LENGTH) {
      log_error(std::string("[WebSocketSession] Session ID too long: ") +
                  std::to_string(provided_session_id.size()) + " > " +
                  std::to_string(config::MAX_SESSION_ID_LENGTH));
      send_protocol_error(protocol::error_codes::PROTOCOL_ERROR, "Session ID exceeds maximum length");
      close();
      return false;
    }

    std::string expected_id = get_session_id_safe();

    if (provided_session_id != expected_id) {
      log_error(std::string("[WebSocketSession] Session ID mismatch: expected ") + sanitize_session_id(expected_id) + ", got " +
                  sanitize_session_id(provided_session_id));
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
    
    // Use string_view to avoid allocation where possible
    // Fallback to empty string if allocation fails
    try {
      err.message = std::string(message);
    } catch (...) {
      err.message = "Error processing message";  // Non-allocating fallback
    }
    
    auto sid = get_session_id_safe();
    if (!sid.empty()) err.session_id = sid;
    
    try {
      if (!send(protocol::serialize_error(err))) {
        log_error("[WebSocketSession] Failed to send protocol error for session " + sanitize_session_id(sid));
      }
    } catch (...) {
      // Non-fatal: failure to log shouldn't propagate
    }
  } catch (...) {
    // Ultimate fallback: don't let any exception escape noexcept function
  }
}

void websocket_session::do_close() noexcept {
  // Two-guard pattern: close_initiated_ CAS prevents double async_close on the
  // same strand; state_ exchange prevents double unregister_session when the
  // close path is entered from different origins (e.g. on_read error vs close()).
  bool expected = false;
  if (!close_initiated_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
    return;
  }

  auto prev_state = state_.exchange(state::closed, std::memory_order_acq_rel);
  if (prev_state == state::closed) {
    return;
  }

  try {
    boost::beast::error_code timer_ec;
    deadline_.cancel(timer_ec);

    std::string session_id_copy = get_session_id_safe();

    if (!session_id_copy.empty()) {
      if (auto mgr = conn_mgr_.lock()) {
        mgr->unregister_session(session_id_copy);
      }
    }

    if (prev_state == state::unauthenticated) {
      // WebSocket handshake was never completed — async_close() requires an
      // open stream (per Beast docs).  Close the underlying TCP socket
      // instead; any pending async_accept will complete with an error.
      boost::beast::error_code close_ec;
      ws_.next_layer().socket().close(close_ec);
      if (close_ec) {
        log_error(std::string("[WebSocketSession] TCP close error: ") + close_ec.message());
      }
    } else {
      // Stream was accepted — send a proper WebSocket close frame.
      ws_.async_close(boost::beast::websocket::close_code::normal,
                      [self = shared_from_this()](boost::beast::error_code ec) {
                        if (ec) {
                          try {
                            log_error(std::string("[WebSocketSession] Close error: ") + ec.message());
                          } catch (...) {
                            // Allocation failure in async handler — log is best-effort.
                          }
                          // Fallback: force-close the TCP socket to prevent FD leak.
                          // Use non-throwing socket close ( Beast's tcp_stream::close()
                          // can throw, but raw socket close with error_code cannot).
                          boost::beast::error_code fallback_ec;
                          self->ws_.next_layer().socket().close(fallback_ec);
                        }
                      });
    }
  } catch (const std::exception& e) {
    try {
      log_error(std::string("[WebSocketSession] Exception in do_close: ") + e.what());
    } catch (...) {
      // Allocation failure in noexcept context — nothing useful to do.
    }
    // Fallback: force-close the TCP socket to prevent FD leak if async_close threw.
    // Use non-throwing socket close directly (Beast's tcp_stream::close() can
    // throw, but raw socket close with error_code cannot).
    boost::beast::error_code fallback_ec;
    ws_.next_layer().socket().close(fallback_ec);
  }
}

// Enhanced error handling implementation
bool websocket_session::send_error(protocol_error error_type,
                                 const std::string& details,
                                 int64_t sequence_number) noexcept {
  try {
    error_context ctx = create_error_context(error_type, details, sequence_number);
    
    nlohmann::json error_json = {
      {"message_type", "ERROR"},
      {"error_code", error_code_to_string(ctx.type)},
      {"message", ctx.details}
    };
    
    if (!ctx.session_id.empty()) {
      error_json["session_id"] = ctx.session_id;
    }
    
    if (ctx.sequence_number >= 0) {
      error_json["sequence_number"] = ctx.sequence_number;
    }
    
    return queue_message(error_json.dump());
  } catch (...) {
    // Allocation failure in noexcept function — fallback to basic protocol error
    return send_protocol_error(protocol::error_codes::PROTOCOL_ERROR, "Internal server error");
  }
}

void websocket_session::add_security_event(const std::string& event) noexcept {
  try {
    std::lock_guard<std::mutex> lock(security_events_mutex_);
    security_events_.push_back(event);
    
    // Keep only recent events to prevent unbounded growth
    if (security_events_.size() > 100) {
      security_events_.erase(security_events_.begin(), 
                           security_events_.begin() + security_events_.size() - 100);
    }
    
    // Log security events at warning level
    log_error(std::string("[WebSocketSession Security] ") + event);
  } catch (...) {
    // Allocation failure — security event was not recorded, but this is non-critical
  }
}

bool websocket_session::check_suspicious_activity() noexcept {
  try {
    // Check for rapid message bursts
    if (message_count_.load() > 100) {
      auto now = std::chrono::steady_clock::now();
      auto time_since_start = std::chrono::duration_cast<std::chrono::seconds>(
          now - last_activity_).count();
      
      if (time_since_start < 5 && message_count_.load() > 50) {
        add_security_event("Rapid message burst detected");
        return true;
      }
    }
    
    // Check for sequence number anomalies
    if (last_sequence_number_.load() > 0) {
      // Simple heuristic: detect unusually large gaps
      if (last_sequence_number_.load() > 1000) {
        add_security_event("Large sequence gap detected");
        return true;
      }
    }
    
    return false;
  } catch (...) {
    // Check failure — assume safe rather than flag false positive
    return false;
  }
}

std::optional<std::string> websocket_session::validate_action(
    const std::string& action_type,
    std::optional<int64_t> amount,
    int64_t current_stack,
    int64_t current_bet) noexcept {
  try {
    // Validate action type
    if (action_type != "FOLD" && action_type != "CHECK" && 
        action_type != "CALL" && action_type != "RAISE" && 
        action_type != "ALL_IN") {
      return "Invalid action type: " + trunc_field(action_type, 32);
    }
    
    // Validate amount for actions that require it
    if (action_type == "RAISE" || action_type == "ALL_IN" || action_type == "CALL") {
      if (!amount.has_value()) {
        return "Amount required for " + action_type + " action";
      }
      
      if (amount.value() <= 0) {
        return "Amount must be positive for " + action_type + " action";
      }
      
      if (amount.value() > protocol::MAX_AMOUNT) {
        return "Amount exceeds maximum allowed value";
      }
      
      if (action_type == "RAISE" && amount.value() <= current_bet) {
        return "Raise amount must be greater than current bet";
      }
      
      if (amount.value() > current_stack) {
        if (action_type != "ALL_IN") {
          return "Amount exceeds available stack";
        }
      }
    }
    
    // Additional validation for ALL_IN
    if (action_type == "ALL_IN" && amount.value() > current_stack) {
      return "ALL_IN amount cannot exceed available stack";
    }
    
    return std::nullopt;  // Validation passed
  } catch (...) {
    return "Internal validation error";
  }
}

bool websocket_session::validate_sequence_number(int64_t received, int64_t expected) noexcept {
  try {
    // Exact match (normal case)
    if (received == expected) {
      return true;
    }
    
    // Allow for small gaps (network reordering)
    if (received > expected && received <= expected + enhanced_config::get_max_sequence_gap()) {
      return true;
    }
    
    // Detect replay attacks (old sequence numbers)
    if (received <= expected) {
      add_security_event(std::string("Possible replay attack: sequence number ") + 
                        std::to_string(received) + " <= " + std::to_string(expected));
      return false;
    }
    
    // Gap too large
    add_security_event(std::string("Sequence number gap too large: received ") + 
                      std::to_string(received) + ", expected ") + std::to_string(expected);
    return false;
  } catch (...) {
    return false;
  }
}

error_context websocket_session::create_error_context(
    protocol_error error_type,
    const std::string& details,
    int64_t sequence_number) const noexcept {
  error_context ctx;
  ctx.type = error_type;
  ctx.details = details;
  ctx.sequence_number = sequence_number;
  ctx.timestamp = std::chrono::steady_clock::now();
  
  try {
    ctx.session_id = get_session_id_safe();
  } catch (...) {
    ctx.session_id = "(unknown)";
  }
  
  return ctx;
}

void websocket_session::handle_rate_limit_exceeded() noexcept {
  try {
    std::string session_id_str = get_session_id_safe();
    std::string id_str = session_id_str.empty() ? "(unauthenticated)" : sanitize_session_id(session_id_str);
    
    log_error(std::string("[WebSocketSession] Rate limit exceeded for session ") + id_str);
    add_security_event("Rate limit exceeded");
    
    send_error(protocol_error::rate_limit_exceeded, 
               "Rate limit exceeded. Please reduce message frequency.");
    close();
  } catch (...) {
    // Fallback for allocation failure
    send_protocol_error(protocol::error_codes::SESSION_CLOSED, "Rate limit exceeded");
    close();
  }
}

}  // namespace server
}  // namespace cppsim

}  // namespace server
}  // namespace cppsim
