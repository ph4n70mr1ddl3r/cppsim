#pragma once

#include "boost_wrapper.hpp"
#include <cstdint>
#include <memory>

#include "connection_manager.hpp"
#include "websocket_session.hpp"

namespace cppsim {
namespace server {

// WebSocket server - accepts connections and creates sessions
  class websocket_server final {
   public:
  websocket_server(boost::asio::io_context& ioc, uint16_t port);
  ~websocket_server();

  void run() noexcept;
  void stop() noexcept;

   private:
  void do_accept();
  void on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket);

  boost::asio::io_context& ioc_;
  boost::asio::ip::tcp::acceptor acceptor_;
  std::shared_ptr<connection_manager> conn_mgr_;
  std::shared_ptr<boost::asio::steady_timer> backoff_timer_;
  std::mutex timer_mutex_;
  std::atomic<bool> initialized_{false};
  std::atomic<bool> alive_{true};
  std::atomic<int> backoff_seconds_{1};
};

}  // namespace server
}  // namespace cppsim
