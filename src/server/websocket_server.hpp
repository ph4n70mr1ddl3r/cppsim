#pragma once

#include "boost_wrapper.hpp"
#include <cstdint>
#include <memory>
#include <iostream>

#include "connection_manager.hpp"
#include "websocket_session.hpp"

namespace cppsim {
namespace server {

// WebSocket server - accepts connections and creates sessions
class websocket_server {
 public:
  websocket_server(boost::asio::io_context& ioc, uint16_t port);

  void run();
  void stop();

 private:
  void do_accept();
  
  template <typename Socket>
  void on_accept(boost::beast::error_code ec, Socket socket) {
      if (ec) {
        std::cerr << "[WebSocketServer] Accept failed: " << ec.message()
                  << std::endl;
      } else {
        // Create a new session for this connection
        // We assume Socket is movable to tcp::socket (or is compatible)
        auto session = std::make_shared<websocket_session>(
            boost::asio::ip::tcp::socket(std::move(socket)), conn_mgr_);

        // Register the session and get its ID
        std::string session_id = conn_mgr_->register_session(session);
        session->set_session_id(session_id);

        // Start the session (perform WebSocket handshake and begin reading)
        session->run();

        std::cout << "[WebSocketServer] New connection accepted as " << session_id
                  << std::endl;
      }

      // Accept the next connection
      do_accept();
  }

  boost::asio::io_context& ioc_;
  boost::asio::ip::tcp::acceptor acceptor_;
  std::shared_ptr<connection_manager> conn_mgr_;
};

}  // namespace server
}  // namespace cppsim
