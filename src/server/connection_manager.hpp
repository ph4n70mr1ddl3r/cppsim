#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace cppsim {
namespace server {

// Forward declaration to break circular dependency with websocket_session.hpp
class websocket_session;

// Manages all active WebSocket sessions
// Thread-safe: All public methods are thread-safe and can be called from any thread
class connection_manager final {
 public:
  connection_manager() = default;

  // Register a new session and return its unique session ID
  // Strong exception guarantee - either succeeds completely or throws without modifying state
  // Returns empty string on session ID collision
  std::string register_session(std::shared_ptr<websocket_session> session);

  // Unregister a session by ID (called on disconnect)
  // No-throw guarantee
  void unregister_session(const std::string& session_id);

  // Get session by ID (returns nullptr if not found)
  // No-throw guarantee
  std::shared_ptr<websocket_session> get_session(const std::string& session_id) const;

  // Get all active session IDs
  // Strong exception guarantee
  std::vector<std::string> active_session_ids() const;

   // Get count of active sessions
  // No-throw guarantee
  size_t session_count() const noexcept;

  // Stop all active sessions
  // No-throw guarantee
  void stop_all();

 private:
  // Generate unique session ID
  // No-throw guarantee
  std::string generate_session_id();

  mutable std::mutex sessions_mutex_;
  std::unordered_map<std::string, std::shared_ptr<websocket_session>>
      sessions_;
  std::atomic<uint64_t> session_counter_{0};
  std::atomic<size_t> active_sessions_{0};
};

}  // namespace server
}  // namespace cppsim
