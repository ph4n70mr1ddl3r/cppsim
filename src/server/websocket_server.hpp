#pragma once

#include "boost_wrapper.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

#include "connection_manager.hpp"

namespace cppsim {
namespace server {

class websocket_session;

class websocket_server final : public std::enable_shared_from_this<websocket_server> {
 public:
  websocket_server(boost::asio::io_context& ioc, uint16_t port);
  websocket_server(const websocket_server&) = delete;
  websocket_server& operator=(const websocket_server&) = delete;
  websocket_server(websocket_server&&) = delete;
  websocket_server& operator=(websocket_server&&) = delete;
  ~websocket_server() noexcept;

  void run() noexcept;
  void stop() noexcept;

  [[nodiscard]] std::shared_ptr<connection_manager> get_connection_manager() const noexcept { return conn_mgr_; }

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
