#include "connection_manager.hpp"

#include <chrono>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <thread>

#include "config.hpp"
#include "logger.hpp"
#include "websocket_session.hpp"

namespace cppsim {
namespace server {

std::string connection_manager::register_session(
    std::shared_ptr<websocket_session> session) {
  std::string session_id = generate_session_id();

  if (session_id.empty()) {
    cppsim::server::log_error("[ConnectionManager] Failed to generate session ID");
    return "";
  }

  if (session_id.length() > config::MAX_SESSION_ID_LENGTH) {
    cppsim::server::log_error("[ConnectionManager] Generated session ID exceeds maximum length");
    return "";
  }

  size_t count;
  {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    if (sessions_.size() >= config::MAX_CONNECTIONS) {
      cppsim::server::log_error("[ConnectionManager] Maximum connections reached (" + 
          std::to_string(config::MAX_CONNECTIONS) + ")");
      return "";
    }
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

void connection_manager::unregister_session(const std::string& session_id) noexcept {
  if (session_id.empty()) {
    return;
  }
  size_t count;
  {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(session_id);
    count = sessions_.size();
    active_sessions_.store(count, std::memory_order_relaxed);
  }

  cppsim::server::log_message("[ConnectionManager] Unregistered session: " + session_id + " (remaining: " +
              std::to_string(count) + ")");
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

  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch()).count();
  
  uint32_t random_part = 0;
  auto compute_fallback = [&]() -> uint32_t {
    auto thread_hash = std::hash<std::thread::id>{}(std::this_thread::get_id());
    auto ts_unsigned = static_cast<uint64_t>(timestamp);
    auto steady_hash = std::hash<typename std::chrono::steady_clock::rep>{}
        (std::chrono::steady_clock::now().time_since_epoch().count());
    uint32_t result = static_cast<uint32_t>(ts_unsigned ^ id ^ 
        static_cast<uint64_t>(thread_hash) ^ static_cast<uint64_t>(steady_hash));
    result ^= (result >> 16);
    result *= 0x85ebca6b;
    result ^= (result >> 13);
    result *= 0xc2b2ae35;
    result ^= (result >> 16);
    return result;
  };

  try {
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, std::numeric_limits<uint32_t>::max());
    random_part = dis(gen);
  } catch (const std::exception& e) {
    cppsim::server::log_error(std::string("[ConnectionManager] Random device failed: ") + e.what() + ", using fallback");
    random_part = compute_fallback();
  } catch (...) {
    cppsim::server::log_error("[ConnectionManager] Random device failed with unknown error, using fallback");
    random_part = compute_fallback();
  }

  return "sess_" + std::to_string(timestamp) + "_" + std::to_string(id) + "_" + std::to_string(random_part);
}

void connection_manager::stop_all() noexcept {
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
