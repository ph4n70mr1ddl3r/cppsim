#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace cppsim {
namespace server {

class websocket_session;

// Thread safety:
//   All public methods are safe to call from any thread.
//   - Mutex-protected: register_session, unregister_session, get_session,
//     active_session_ids, session_count, empty, stop_all.
//   - Sessions are shared_ptr; callers must synchronize access to the
//     session object itself (websocket_session handles its own thread safety).
class connection_manager final {
 public:
  connection_manager() noexcept = default;
  ~connection_manager() noexcept = default;

  connection_manager(const connection_manager&) = delete;
  connection_manager& operator=(const connection_manager&) = delete;
  connection_manager(connection_manager&&) = delete;
  connection_manager& operator=(connection_manager&&) = delete;

  // Returns empty string on failure (null session, max connections reached,
  // or session ID collision after all retries).
  [[nodiscard]] std::string register_session(std::shared_ptr<websocket_session> session);

  void unregister_session(std::string_view session_id) noexcept;

  [[nodiscard]] std::shared_ptr<websocket_session> get_session(std::string_view session_id) const noexcept;

  // Note: Can throw std::bad_alloc on memory exhaustion.
  [[nodiscard]] std::vector<std::string> active_session_ids() const;

  [[nodiscard]] size_t session_count() const noexcept;

  void stop_all() noexcept;

 private:
  [[nodiscard]] static std::string generate_session_id() noexcept;

  mutable std::mutex sessions_mutex_;
  std::map<std::string, std::shared_ptr<websocket_session>, std::less<>> sessions_;
};

}  // namespace server
}  // namespace cppsim
