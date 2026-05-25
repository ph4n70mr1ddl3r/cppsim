#pragma once

#include "boost_wrapper.hpp"
#include "connection_manager.hpp"
#include "session_metrics.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <vector>

#include "config.hpp"
#include "protocol.hpp"

namespace cppsim {
namespace server {

/**
 * @brief Enhanced error types for protocol-level errors
 * 
 * Provides structured error categorization for better debugging and client feedback.
 * Each error type includes context information for detailed error reporting.
 */
enum class protocol_error {
    invalid_session_id,         ///< Invalid or malformed session ID
    sequence_number_mismatch,   ///< Sequence number out of order
    invalid_action_type,       ///< Unknown or invalid action type
    insufficient_funds,        ///< Insufficient balance for action
    amount_out_of_bounds,       ///< Amount exceeds maximum limits
    malformed_message,         ///< JSON parsing failed or missing required fields
    rate_limit_exceeded,       ///< Client exceeded message rate limit
    server_internal_error,     ///< Internal server error (should be rare)
    authentication_failed      ///< Handshake or authentication failed
};

/**
 * @brief Context information for protocol errors
 * 
 * Contains detailed error information including session ID, sequence number,
 * and timestamp for debugging and logging purposes.
 */
struct error_context {
    protocol_error type;                                ///< Type of error
    std::string details;                               ///< Human-readable error details
    std::string session_id;                             ///< Associated session ID (if available)
    int64_t sequence_number = -1;                       ///< Sequence number (if available)
    std::chrono::steady_clock::time_point timestamp;    ///< Error timestamp
    std::string action_type;                            ///< Action type (if applicable)
    std::optional<int64_t> amount;                      ///< Amount (if applicable)
};



/**
 * @class websocket_session
 * @brief Manages individual WebSocket connections for poker game sessions
 * 
 * Provides thread-safe connection handling, message processing, authentication,
 * and lifecycle management for WebSocket connections. Handles both handshake
 * and authenticated message phases with proper error handling and rate limiting.
 * 
 * Thread Safety Model:
 * - Strand-only state (no lock needed): current_stack_, handshake_timeout_
 *   — accessed only from handlers dispatched to the session's strand.
 * - Mutex-protected state: session_id_ (session_id_mutex_), write_queue_ +
 *   writing_ (write_queue_mutex_), message_timestamps_ (rate_limit_mutex_).
 * - Atomic state: state_, last_sequence_number_, close_requested_,
 *   close_initiated_.
 * - All public methods (send, close, is_authenticated, session_id) are safe
 *   to call from any thread; they either use atomics, mutexes, or dispatch
 *   to the strand internally.
 */
class websocket_session final
    : public std::enable_shared_from_this<websocket_session> {
 public:
  websocket_session(boost::asio::ip::tcp::socket socket,
                    std::shared_ptr<connection_manager> mgr,
                    std::chrono::seconds handshake_timeout = config::HANDSHAKE_TIMEOUT);
  ~websocket_session() noexcept;

  void run() noexcept;

  [[nodiscard]] bool send(std::string message);
  
  /**
   * @brief Send a structured error message to the client
   * @param error_type Type of protocol error
   * @param details Human-readable error details
   * @param sequence_number Sequence number context (if applicable)
   * @return true if error message was queued successfully, false otherwise
   * 
   * Thread Safety: Safe to call from any thread. Queues error message
   * with proper thread synchronization. Returns false only if write queue is full.
   */
  [[nodiscard]] bool send_error(protocol_error error_type,
                             const std::string& details,
                             int64_t sequence_number = -1) noexcept;

  [[nodiscard]] bool is_authenticated() const noexcept {
    return state_.load(std::memory_order_acquire) == state::authenticated;
  }

  /**
   * @brief Gracefully close the WebSocket connection
   * 
   * Sets close_requested_ flag and dispatches cleanup to the strand.
   * Thread-safe to call from any thread.
   */
  void close() noexcept;

  [[nodiscard]] std::string session_id() const noexcept {
    return get_session_id_safe();
  }

  websocket_session(const websocket_session&) = delete;
  websocket_session& operator=(const websocket_session&) = delete;
  websocket_session(websocket_session&&) = delete;
  websocket_session& operator=(websocket_session&&) = delete;

 private:
  void do_accept();
  void on_accept(boost::beast::error_code ec);

  void do_read();
  void on_read(boost::beast::error_code ec, std::size_t bytes_transferred);

  void do_write();
  void on_write(boost::beast::error_code ec, std::size_t bytes_transferred);

  void check_deadline();

  // Validates the provided session_id against this session's stored ID.
  // On failure: sends a PROTOCOL_ERROR to the client and calls close().
  // Callers should return immediately if this returns false.
  [[nodiscard]] bool validate_session_id(const std::string& provided_session_id) noexcept;
  void send_protocol_error(const char* error_code, std::string_view message) noexcept;
  void do_close() noexcept;

  [[nodiscard]] bool check_rate_limit_or_close() noexcept;
  
  /**
   * @brief Add security event for audit logging
   * @param event Description of security-related event
   * 
   * Thread Safety: Safe to call from any thread. Handles allocation failures
   * gracefully to prevent std::terminate in noexcept context.
   */
  void add_security_event(const std::string& event) noexcept;
  
  /**
   * @brief Check for suspicious activity patterns
   * @return true if activity appears suspicious, false otherwise
   * 
   * Monitors message frequency, sequence number patterns, and other
   * potential security indicators.
   */
  [[nodiscard]] bool check_suspicious_activity() noexcept;
  
  void handle_handshake_message(const std::string& message);
  void handle_authenticated_message(const std::string& message);
  void handle_action(const protocol::parsed_message_header& header, const std::string& sid);
  void handle_reload_msg(const protocol::parsed_message_header& header, const std::string& sid);
  void handle_disconnect_msg(const protocol::parsed_message_header& header, const std::string& sid);

  [[nodiscard]] bool queue_message(std::string&& message) noexcept;
  [[nodiscard]] std::string get_session_id_safe() const noexcept;

  error_context create_error_context(
      protocol_error error_type,
      const std::string& details,
      int64_t sequence_number = -1) const noexcept;

  boost::beast::websocket::stream<boost::beast::tcp_stream> ws_;
  boost::beast::flat_buffer buffer_;
  std::string session_id_;
  mutable std::mutex session_id_mutex_;
  std::weak_ptr<connection_manager> conn_mgr_;
  std::queue<std::string> write_queue_;
  mutable std::mutex write_queue_mutex_;
  bool writing_{false};  // Protected by write_queue_mutex_. Indicates async_write in flight.

  enum class state { unauthenticated, authenticated, closed };
  std::atomic<state> state_{state::unauthenticated};

  boost::asio::steady_timer deadline_;

  std::atomic<int64_t> last_sequence_number_{-1};

  std::atomic<bool> close_requested_{false};
  std::atomic<bool> close_initiated_{false};

  std::deque<std::chrono::steady_clock::time_point> message_timestamps_;
  mutable std::mutex rate_limit_mutex_;
  
  // Security monitoring
  std::chrono::steady_clock::time_point last_activity_;
  std::atomic<int64_t> message_count_{0};
  std::deque<std::string> security_events_;
  mutable std::mutex security_events_mutex_;
  
  // Performance metrics
  session_metrics metrics_;

  // Mutable state — only accessed from the session's strand (single-threaded).
  // No mutex needed: all reads/writes happen in handlers dispatched to the
  // strand executor (do_read -> on_read -> handle_* -> do_write -> on_write).
  int64_t current_stack_{config::PLACEHOLDER_STACK};
  std::chrono::seconds handshake_timeout_;
};

}  // namespace server
}  // namespace cppsim
