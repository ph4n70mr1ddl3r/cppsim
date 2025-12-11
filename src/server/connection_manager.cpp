#include "connection_manager.hpp"

#include <iostream>

#include "websocket_session.hpp"

namespace cppsim {
namespace server {

std::string connection_manager::register_session(
    std::shared_ptr<websocket_session> session) {
  std::string session_id = generate_session_id();

  {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_[session_id] = session;
  }

  std::cout << "[ConnectionManager] Registered session: " << session_id
            << " (total: " << session_count() << ")" << std::endl;

  return session_id;
}

void connection_manager::unregister_session(const std::string& session_id) {
  {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(session_id);
  }

  std::cout << "[ConnectionManager] Unregistered session: " << session_id
            << " (remaining: " << session_count() << ")" << std::endl;
}

std::shared_ptr<websocket_session> connection_manager::get_session(
    const std::string& session_id) {
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

size_t connection_manager::session_count() const {
  std::lock_guard<std::mutex> lock(sessions_mutex_);
  return sessions_.size();
}

std::string connection_manager::generate_session_id() {
  uint64_t id = session_counter_.fetch_add(1, std::memory_order_relaxed);
  return "session_" + std::to_string(id + 1);  // Start from session_1
}

void connection_manager::stop_all() {
  std::lock_guard<std::mutex> lock(sessions_mutex_);
  for (auto& pair : sessions_) {
    pair.second->close();
  }
  sessions_.clear();
  std::cout << "[ConnectionManager] All sessions stopped." << std::endl;
}

}  // namespace server
}  // namespace cppsim
