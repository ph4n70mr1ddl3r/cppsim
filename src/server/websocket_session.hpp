#pragma once

#include "boost_wrapper.hpp"
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

  // Start the session (performs WebSocket handshake and begins reading)
  void run();

  // Send a message to the client (queued if write in progress)
  void send(const std::string& message);

  // Gracefully close the WebSocket connection
  void close();

  // Get the session ID
  std::string session_id() const { return session_id_; }

  // Set the session ID (called by connection_manager after registration)
  void set_session_id(const std::string& id) { session_id_ = id; }

 private:
  void do_accept();
  void on_accept(boost::beast::error_code ec);

  void do_read();
  void on_read(boost::beast::error_code ec, std::size_t bytes_transferred);

  void do_write();
  void on_write(boost::beast::error_code ec, std::size_t bytes_transferred);

  boost::beast::websocket::stream<
      boost::beast::tcp_stream>
      ws_;
  boost::beast::flat_buffer buffer_;
  std::string session_id_;
  std::shared_ptr<connection_manager> conn_mgr_;
  std::queue<std::string> write_queue_;
  bool writing_{false};
};

}  // namespace server
}  // namespace cppsim
