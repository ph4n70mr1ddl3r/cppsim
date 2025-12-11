#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace cppsim {
namespace server {

// Forward declaration
class websocket_session;

// Manages all active WebSocket sessions
// Thread-safe session registration, lookup, and cleanup
class connection_manager {
 public:
  connection_manager() = default;

  // Register a new session and return its unique session ID
  std::string register_session(std::shared_ptr<websocket_session> session);

  // Unregister a session by ID (called on disconnect)
  void unregister_session(const std::string& session_id);

  // Get session by ID (returns nullptr if not found)
  std::shared_ptr<websocket_session> get_session(
      const std::string& session_id);

  // Get all active session IDs
  std::vector<std::string> active_session_ids() const;

  // Get count of active sessions
  size_t session_count() const;

 private:
  // Generate unique session ID
  std::string generate_session_id();

  mutable std::mutex sessions_mutex_;
  std::unordered_map<std::string, std::shared_ptr<websocket_session>>
      sessions_;
  std::atomic<uint64_t> session_counter_{0};
};

}  // namespace server
}  // namespace cppsim
