#pragma once

#include "boost_wrapper.hpp"
#include <atomic>
#include <memory>
#include <queue>
#include <string>

namespace cppsim {
namespace server {

// Forward declaration
class connection_manager;

// Represents a single WebSocket client connection
// Handles async read/write operations and session lifecycle
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

  // Gracefully close the WebSocket connection
  void close();

  // Get the session ID
  std::string session_id() const noexcept { return session_id_; }

  // Delete copy and move operations to prevent accidental copying
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
  std::weak_ptr<connection_manager> conn_mgr_;
  std::queue<std::string> write_queue_;
  bool writing_{false};

  // Session state
  enum class state { unauthenticated, authenticated, closed };
  std::atomic<state> state_{state::unauthenticated};

  // Authentication timeout
  boost::asio::steady_timer deadline_;
  
  // Graceful closure support
  bool should_close_{false};
  void do_close();
};

}  // namespace server
}  // namespace cppsim
