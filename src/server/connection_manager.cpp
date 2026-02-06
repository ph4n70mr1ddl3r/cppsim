#include "connection_manager.hpp"

#include <limits>

#include "logger.hpp"
#include "websocket_session.hpp"

namespace cppsim {
namespace server {

std::string connection_manager::register_session(
    std::shared_ptr<websocket_session> session) {
  std::string session_id = generate_session_id();

  size_t count;
  {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto result = sessions_.emplace(session_id, session);
    if (!result.second) {
      cppsim::server::log_error("[ConnectionManager] Session ID collision: " + session_id);
      return "";
    }
    count = sessions_.size();
    active_sessions_.store(count, std::memory_order_relaxed);
  }

  cppsim::server::log_message("[ConnectionManager] Registered session: " + session_id + " (total: " + std::to_string(count) + ")");

  return session_id;
}

void connection_manager::unregister_session(const std::string& session_id) {
  size_t count;
  {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(session_id);
    count = sessions_.size();
    active_sessions_.store(count, std::memory_order_relaxed);
  }

  if (!session_id.empty()) {
    cppsim::server::log_message("[ConnectionManager] Unregistered session: " + session_id + " (remaining: " +
                std::to_string(count) + ")");
  }
}

std::shared_ptr<websocket_session> connection_manager::get_session(
    const std::string& session_id) const {
  std::lock_guard<std::mutex> lock(sessions_mutex_);
  auto it = sessions_.find(session_id);
  if (it != sessions_.end()) {
    return it->second;
  }
  return nullptr;
}

std::vector<std::string> connection_manager::active_session_ids() const {
  std::lock_guard<std::mutex> lock(sessions_mutex_);
  std::vector<std::string> ids;
  ids.reserve(sessions_.size());
  for (const auto& pair : sessions_) {
    ids.push_back(pair.first);
  }
  return ids;
}

size_t connection_manager::session_count() const noexcept {
  return active_sessions_.load(std::memory_order_relaxed);
}

std::string connection_manager::generate_session_id() {
  uint64_t id = session_counter_.fetch_add(1, std::memory_order_relaxed);

  if (id == std::numeric_limits<uint64_t>::max()) {
    cppsim::server::log_error("[ConnectionManager] Session counter overflow, wrapping around");
  }

  std::string session_id = "session_" + std::to_string(id + 1);

  return session_id;
}

void connection_manager::stop_all() {
  std::vector<std::shared_ptr<websocket_session>> sessions_to_stop;
  {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_to_stop.reserve(sessions_.size());
    for (auto& pair : sessions_) {
      sessions_to_stop.push_back(pair.second);
    }
    sessions_.clear();
    active_sessions_.store(0, std::memory_order_relaxed);
  }

  for (auto& session : sessions_to_stop) {
    session->close();
  }

  cppsim::server::log_message("[ConnectionManager] All sessions stopped.");
}

}  // namespace server
}  // namespace cppsim
