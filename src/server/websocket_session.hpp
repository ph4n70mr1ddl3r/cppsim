#pragma once

#include "boost_wrapper.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>

#include "config.hpp"
#include <nlohmann/json.hpp>

namespace cppsim {
namespace server {

class connection_manager;

class websocket_session final
    : public std::enable_shared_from_this<websocket_session> {
 public:
  websocket_session(boost::asio::ip::tcp::socket socket,
                    std::shared_ptr<connection_manager> mgr);

  ~websocket_session() noexcept;

  void run();

  [[nodiscard]] bool send(std::string message);

  [[nodiscard]] bool is_authenticated() const noexcept {
    return state_.load(std::memory_order_acquire) == state::authenticated;
  }

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

  [[nodiscard]] bool validate_session_id(const std::string& provided_session_id) noexcept;
  void send_protocol_error(const char* error_code, std::string_view message) noexcept;
  void do_close() noexcept;

  [[nodiscard]] bool check_rate_limit();
  void handle_handshake_message(const std::string& message);
  void handle_authenticated_message(const std::string& message);
  void handle_action(const nlohmann::json& envelope_json, const std::string& sid);
  void handle_reload_msg(const nlohmann::json& envelope_json, const std::string& sid);
  void handle_disconnect_msg(const nlohmann::json& envelope_json, const std::string& sid);

  [[nodiscard]] bool queue_message(std::string&& message);
  [[nodiscard]] std::string get_session_id_safe() const noexcept;

  boost::beast::websocket::stream<boost::beast::tcp_stream> ws_;
  boost::beast::flat_buffer buffer_;
  std::string session_id_;
  mutable std::mutex session_id_mutex_;
  std::weak_ptr<connection_manager> conn_mgr_;
  std::queue<std::string> write_queue_;
  mutable std::mutex write_queue_mutex_;
  bool writing_{false};  // INVARIANT: all accesses must hold write_queue_mutex_

  enum class state { unauthenticated, authenticated, closed };
  std::atomic<state> state_{state::unauthenticated};

  boost::asio::steady_timer deadline_;

  std::atomic<int64_t> last_sequence_number_{-1};

  std::atomic<bool> close_requested_{false};
  std::atomic<bool> close_initiated_{false};

  std::deque<std::chrono::steady_clock::time_point> message_timestamps_;
  mutable std::mutex rate_limit_mutex_;

  double current_stack_{config::PLACEHOLDER_STACK};
};

}  // namespace server
}  // namespace cppsim
