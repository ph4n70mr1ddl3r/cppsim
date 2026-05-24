#include "connection_manager.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string_view>
#include <utility>

#include "config.hpp"
#include "logger.hpp"
#include "sanitize.hpp"
#include "websocket_session.hpp"

namespace {

constexpr int MAX_SESSION_ID_RETRIES = 3;

}  // namespace

// Anonymous namespace helpers reference log_error/log_message via
// cppsim::server:: qualified calls.

namespace cppsim {
namespace server {

std::string connection_manager::register_session(
    std::shared_ptr<websocket_session> session) {
  if (!session) {
    log_error("[ConnectionManager] Cannot register null session");
    return std::string();
  }
  
  for (int attempt = 0; attempt < MAX_SESSION_ID_RETRIES; ++attempt) {
    std::string session_id = generate_session_id();

    if (session_id.empty()) {
      log_error("[ConnectionManager] Failed to generate session ID");
      return std::string();
    }

    if (session_id.size() > config::MAX_SESSION_ID_LENGTH) {
      log_error("[ConnectionManager] Generated session ID exceeds maximum length");
      return std::string();
    }

    // emplace_result distinguishes the three outcomes so logging can happen
    // outside the lock, reducing sessions_mutex_ hold time on the hot path.
    enum class emplace_result { success, max_connections, collision };
    auto result = emplace_result::success;
    size_t count = 0;
    {
      std::lock_guard<std::mutex> lock(sessions_mutex_);
      if (sessions_.size() >= config::MAX_CONNECTIONS) {
        result = emplace_result::max_connections;
      } else {
        // session is moved via try_emplace — on collision the forwarded arg is
        // untouched (C++17 guarantee), so the local parameter remains valid for
        // retry.  On success, ownership transfers into the map without an extra
        // atomic refcount increment/decrement pair.
        auto [it, inserted] = sessions_.try_emplace(session_id, std::move(session));
        if (!inserted) {
          result = emplace_result::collision;
        } else {
          count = sessions_.size();
        }
      }
    }

    switch (result) {
      case emplace_result::max_connections:
        log_error("[ConnectionManager] Maximum connections reached (" +
            std::to_string(config::MAX_CONNECTIONS) + ")");
        return std::string();
      case emplace_result::collision:
        log_error("[ConnectionManager] Session ID collision (attempt " +
            std::to_string(attempt + 1) + "), retrying: " + cppsim::server::sanitize_session_id(session_id));
        continue;
      case emplace_result::success:
        break;
      default:
        log_error("[ConnectionManager] Unexpected emplace result");
        return std::string();
    }

    log_message("[ConnectionManager] Registered session: " + cppsim::server::sanitize_session_id(session_id) + " (total: " + std::to_string(count) + ")");

    return session_id;
  }

  log_error("[ConnectionManager] Session ID collision after all retries");
  return std::string();
}

void connection_manager::unregister_session(std::string_view session_id) noexcept {
  if (session_id.empty()) {
    return;
  }
  size_t count;
  bool found = false;
  {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
      sessions_.erase(it);
      found = true;
    }
    count = sessions_.size();
  }
  if (found) {
    // Log message construction is wrapped in try/catch because this function
    // is noexcept — an allocation failure in string concatenation would
    // otherwise call std::terminate.
    try {
      log_message("[ConnectionManager] Unregistered session: " + cppsim::server::sanitize_session_id(session_id) + " (remaining: " +
                  std::to_string(count) + ")");
    } catch (...) {
      // Allocation failure — session was still unregistered successfully.
    }
  }
}

std::shared_ptr<websocket_session> connection_manager::get_session(
    std::string_view session_id) const noexcept {
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
  std::lock_guard<std::mutex> lock(sessions_mutex_);
  return sessions_.size();
}

bool connection_manager::empty() const noexcept {
  std::lock_guard<std::mutex> lock(sessions_mutex_);
  return sessions_.empty();
}

std::string connection_manager::generate_session_id() noexcept {
  try {
    static std::atomic<uint64_t> counter{0};
    uint64_t id = counter.fetch_add(1, std::memory_order_relaxed);
    return "sess_" + std::to_string(id);
  } catch (const std::exception& e) {
    // Avoid allocation in noexcept function - use string_view and separate calls
    try {
      log_error("[ConnectionManager] Failed to generate session ID");
    } catch (...) {
      // Ultimate fallback
    }
    return std::string();
  }
}

void connection_manager::stop_all() noexcept {
  decltype(sessions_) sessions_to_stop;
  {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_to_stop.swap(sessions_);
    // swap is O(1) — moves the entire red-black tree root instead of
    // iterating and moving each element individually.
  }

  const size_t count = sessions_to_stop.size();
  for (auto& [id, session] : sessions_to_stop) {
    (void)id;
    session->close();
  }

  try {
    // Use snprintf to avoid allocations in noexcept function
    char buffer[128];
    int len = std::snprintf(buffer, sizeof(buffer), 
                             "[ConnectionManager] Stopped %zu session(s).", count);
    if (len > 0 && len < static_cast<int>(sizeof(buffer))) {
      log_message(std::string_view(buffer, static_cast<size_t>(len)));
    }
  } catch (...) {
    // Allocation failure — sessions were still stopped successfully.
  }
}

}  // namespace server
}  // namespace cppsim
