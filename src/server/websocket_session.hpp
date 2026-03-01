#pragma once

#include "boost_wrapper.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

namespace cppsim {
namespace server {

// Forward declaration
class connection_manager;

// Represents a single WebSocket client connection
// Handles async read/write operations and session lifecycle
// Thread-safe for all public methods
class websocket_session
    : public std::enable_shared_from_this<websocket_session> {
 public:
  websocket_session(boost::asio::ip::tcp::socket socket,
                    std::shared_ptr<connection_manager> mgr);

  ~websocket_session();

  // Start the session (performs WebSocket handshake and begins reading)
  void run();

  // Send a message to the client (queued if write in progress)
  void send(const std::string& message);
  void send(std::string&& message);

  // Gracefully close the WebSocket connection
  void close();

  // Get the session ID (thread-safe)
  std::string session_id() const noexcept { 
    std::lock_guard<std::mutex> lock(session_id_mutex_);
    return session_id_;
  }

  // Delete copy and move operations to prevent accidental copying
  // websocket_session manages async operations and should only be accessed via shared_ptr
  websocket_session(const websocket_session&) = delete;
  websocket_session& operator=(const websocket_session&) = delete;
  websocket_session(websocket_session&&) = delete;
  websocket_session& operator=(websocket_session&&) = delete;

 private:
  bool validate_session_id(const std::string& provided_session_id);
  void do_accept();
  void on_accept(boost::beast::error_code ec);

  void do_read();
  void on_read(boost::beast::error_code ec, std::size_t bytes_transferred);

  void do_write();
  void on_write(boost::beast::error_code ec, std::size_t bytes_transferred);

  void check_deadline();

  boost::beast::websocket::stream<boost::beast::tcp_stream> ws_;
  boost::beast::flat_buffer buffer_;
  std::string session_id_;
  mutable std::mutex session_id_mutex_;
  std::weak_ptr<connection_manager> conn_mgr_;
  std::queue<std::string> write_queue_;
  mutable std::mutex write_queue_mutex_;
  std::atomic<bool> writing_{false};

  // Session state
  enum class state { unauthenticated, authenticated, closed };
  std::atomic<state> state_{state::unauthenticated};

  // Authentication timeout
  boost::asio::steady_timer deadline_;

  // Graceful closure support
  std::atomic<bool> should_close_{false};
  void do_close();

  // Private helper for queuing messages
  void queue_message(std::string&& message);

  // Sequence number tracking for replay attack prevention
  std::atomic<int> last_sequence_number_{-1};

  // Rate limiting for DoS prevention
  // Sliding window using timestamps of recent messages
  std::vector<std::chrono::steady_clock::time_point> message_timestamps_;
  mutable std::mutex rate_limit_mutex_;
  static constexpr std::chrono::milliseconds RATE_LIMIT_WINDOW{1000};  // 1 second window
};

}  // namespace server
}  // namespace cppsim
