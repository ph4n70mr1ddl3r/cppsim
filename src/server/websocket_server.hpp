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
  
  void on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket) {
      if (ec) {
        std::cerr << "[WebSocketServer] Accept failed: " << ec.message()
                  << std::endl;
        
        // Prevent infinite loop on persistent errors (e.g. EMFILE)
        if (!acceptor_.is_open()) return;

        // Backoff: Wait 1s before retrying
        auto timer = std::make_shared<boost::asio::steady_timer>(ioc_);
        timer->expires_after(std::chrono::seconds(1));
        timer->async_wait([this, timer](boost::beast::error_code timer_ec) {
            if (!timer_ec && acceptor_.is_open()) {
                do_accept();
            }
        });
        return;
      } else {
        // Create a new session for this connection
        auto session = std::make_shared<websocket_session>(
            std::move(socket), conn_mgr_);

        // Start the session (perform WebSocket handshake and begin reading)
        session->run();

        std::cout << "[WebSocketServer] New connection accepted" << std::endl;
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
