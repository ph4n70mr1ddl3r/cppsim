#include "connection_manager.hpp"

#include <chrono>
#include <cstdint>
#include <limits>
#include <random>
#include <thread>

#include "config.hpp"
#include "logger.hpp"
#include "websocket_session.hpp"

namespace {

std::mt19937& get_thread_local_generator() {
  static thread_local std::mt19937 gen{[] {
    std::random_device rd;
    std::seed_seq seq{rd(), rd(), rd(), rd(), rd()};
    return std::mt19937{seq};
  }()};
  return gen;
}

uint32_t compute_fallback_random(uint64_t timestamp, uint64_t id) {
  // MurmurHash3 finalizer mixing constants for better bit distribution
  // These constants are derived from MurmurHash3's finalization step
  auto thread_hash = std::hash<std::thread::id>{}(std::this_thread::get_id());
  auto steady_hash = std::hash<typename std::chrono::steady_clock::rep>{}
      (std::chrono::steady_clock::now().time_since_epoch().count());
  uint32_t result = static_cast<uint32_t>(timestamp ^ id ^ 
      static_cast<uint64_t>(thread_hash) ^ static_cast<uint64_t>(steady_hash));
  // MurmurHash3 finalizer: improves avalanche behavior
  result ^= (result >> 16);
  result *= 0x85ebca6b;
  result ^= (result >> 13);
  result *= 0xc2b2ae35;
  result ^= (result >> 16);
  return result;
}

}  // namespace

namespace cppsim {
namespace server {

std::string connection_manager::register_session(
    std::shared_ptr<websocket_session> session) {
  if (!session) {
    log_error("[ConnectionManager] Cannot register null session");
    return "";
  }
  constexpr int max_retries = 3;
  
  for (int attempt = 0; attempt < max_retries; ++attempt) {
    std::string session_id = generate_session_id();

    if (session_id.empty()) {
      log_error("[ConnectionManager] Failed to generate session ID");
      return "";
    }

    if (session_id.length() > config::MAX_SESSION_ID_LENGTH) {
      log_error("[ConnectionManager] Generated session ID exceeds maximum length");
      return "";
    }

    size_t count;
    {
      std::lock_guard<std::mutex> lock(sessions_mutex_);
      if (sessions_.size() >= config::MAX_CONNECTIONS) {
        log_error("[ConnectionManager] Maximum connections reached (" + 
            std::to_string(config::MAX_CONNECTIONS) + ")");
        return "";
      }
      auto result = sessions_.emplace(session_id, session);
      if (!result.second) {
        if (attempt < max_retries - 1) {
          log_error("[ConnectionManager] Session ID collision (attempt " + 
              std::to_string(attempt + 1) + "), retrying: " + session_id);
          continue;
        }
        log_error("[ConnectionManager] Session ID collision after retries: " + session_id);
        return "";
      }
      count = sessions_.size();
    }

    log_message("[ConnectionManager] Registered session: " + session_id + " (total: " + std::to_string(count) + ")");

    return session_id;
  }
  
  return "";
}

void connection_manager::unregister_session(std::string_view session_id) noexcept {
  if (session_id.empty()) {
    return;
  }
  unregister_session_impl(std::string(session_id));
}

void connection_manager::unregister_session(std::string&& session_id) noexcept {
  if (session_id.empty()) {
    return;
  }
  unregister_session_impl(std::move(session_id));
}

void connection_manager::unregister_session_impl(std::string&& session_id) noexcept {
  std::string id_for_log = session_id;
  size_t count;
  {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(session_id);
    count = sessions_.size();
  }

  log_message("[ConnectionManager] Unregistered session: " + id_for_log + " (remaining: " +
              std::to_string(count) + ")");
}

std::shared_ptr<websocket_session> connection_manager::get_session(
    std::string_view session_id) const noexcept {
  std::lock_guard<std::mutex> lock(sessions_mutex_);
  auto it = sessions_.find(std::string(session_id));
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

std::string connection_manager::generate_session_id() {
  uint64_t id = session_counter_.fetch_add(1, std::memory_order_relaxed);

  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch()).count();
  
  uint32_t random_part = 0;

  try {
    std::uniform_int_distribution<uint32_t> dis(
        std::numeric_limits<uint32_t>::min(),
        std::numeric_limits<uint32_t>::max());
    random_part = dis(get_thread_local_generator());
  } catch (...) {
    log_error("[ConnectionManager] Random device failed, using fallback");
    random_part = compute_fallback_random(static_cast<uint64_t>(timestamp), id);
  }

  try {
    return "sess_" + std::to_string(timestamp) + "_" + std::to_string(id) + "_" + std::to_string(random_part);
  } catch (const std::exception& e) {
    log_error(std::string("[ConnectionManager] Failed to generate session ID string: ") + e.what());
    return "";
  }
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
  }

  for (auto& session : sessions_to_stop) {
    session->close();
  }

  log_message("[ConnectionManager] All sessions stopped.");
}

}  // namespace server
}  // namespace cppsim
