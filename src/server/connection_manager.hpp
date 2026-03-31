#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace cppsim {
namespace server {

class websocket_session;

class connection_manager final {
 public:
  connection_manager() noexcept = default;
  ~connection_manager() noexcept = default;

  [[nodiscard]] std::string register_session(std::shared_ptr<websocket_session> session);

  void unregister_session(std::string_view session_id) noexcept;

  [[nodiscard]] std::shared_ptr<websocket_session> get_session(std::string_view session_id) const noexcept;

  [[nodiscard]] std::vector<std::string> active_session_ids() const;

  [[nodiscard]] size_t session_count() const noexcept;

  void stop_all() noexcept;

 private:
  [[nodiscard]] std::string generate_session_id();

  mutable std::mutex sessions_mutex_;
  std::map<std::string, std::shared_ptr<websocket_session>, std::less<>> sessions_;
  std::atomic<uint64_t> session_counter_{0};
};

}  // namespace server
}  // namespace cppsim
