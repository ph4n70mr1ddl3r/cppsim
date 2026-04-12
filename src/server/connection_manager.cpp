#include "connection_manager.hpp"

#include <array>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <mutex>
#include <thread>

#include "config.hpp"
#include "logger.hpp"
#include "websocket_session.hpp"

namespace {

// Truncate session ID for safe logging (show only first 8 chars of the hex part)
std::string sanitize_session_id(std::string_view sid) {
  if (sid.size() <= 13) return std::string(sid);  // "sess_" + 8 hex chars minimum
  return std::string(sid.substr(0, 13)) + "...";
}

// Generate cryptographically secure random bytes from /dev/urandom (Linux) or arc4random (BSD/macOS).
// Falls back to a hash-mixed entropy source if OS CPRNG is unavailable.
bool get_secure_random(unsigned char* buf, size_t len) noexcept {
#ifdef __linux__
  std::FILE* f = std::fopen("/dev/urandom", "rb");
  if (!f) {
    return false;
  }
  size_t read = std::fread(buf, 1, len, f);
  std::fclose(f);
  return read == len;
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
  arc4random_buf(buf, len);
  return true;
#else
  return false;
#endif
}

// Fallback entropy source when OS CPRNG is unavailable.
// Uses mixed timestamps, addresses, and counter as entropy.
std::string generate_fallback_session_id() {
  static std::atomic<uint64_t> fallback_counter{0};

  uint64_t counter = fallback_counter.fetch_add(1, std::memory_order_relaxed);
  auto steady = std::chrono::steady_clock::now().time_since_epoch().count();
  auto system = std::chrono::system_clock::now().time_since_epoch().count();
  auto thread_hash = std::hash<std::thread::id>{}(std::this_thread::get_id());

  // Mix entropy through MurmurHash3 finalizer
  uint64_t h = counter ^ static_cast<uint64_t>(steady) ^ static_cast<uint64_t>(system) ^
               static_cast<uint64_t>(thread_hash);
  h ^= h >> 33;
  h *= 0xff51afd7ed558ccdULL;
  h ^= h >> 33;
  h *= 0xc4ceb9fe1a85ec53ULL;
  h ^= h >> 33;

  std::array<char, 33> buf;
  std::snprintf(buf.data(), buf.size(), "%016" PRIx64, h);
  return "sess_" + std::string(buf.data());
}

// Generate a cryptographically random session ID: sess_{32 hex chars} = 128 bits of entropy.
std::string generate_crypto_session_id() {
  std::array<unsigned char, 16> random_bytes{};  // 128 bits

  if (!get_secure_random(random_bytes.data(), random_bytes.size())) {
    cppsim::server::log_error("[ConnectionManager] CSPRNG unavailable, using fallback session ID");
    return generate_fallback_session_id();
  }

  std::array<char, 33> hex;
  static constexpr char hex_chars[] = "0123456789abcdef";
  for (size_t i = 0; i < random_bytes.size(); ++i) {
    hex[i * 2] = hex_chars[(random_bytes[i] >> 4) & 0x0f];
    hex[i * 2 + 1] = hex_chars[random_bytes[i] & 0x0f];
  }
  hex[32] = '\0';

  return "sess_" + std::string(hex.data());
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
      auto result = sessions_.try_emplace(session_id, session);
      if (!result.second) {
        if (attempt < max_retries - 1) {
          log_error("[ConnectionManager] Session ID collision (attempt " +
              std::to_string(attempt + 1) + "), retrying: " + sanitize_session_id(session_id));
          continue;
        }
        log_error("[ConnectionManager] Session ID collision after retries: " + sanitize_session_id(session_id));
        return "";
      }
      count = sessions_.size();
    }

    log_message("[ConnectionManager] Registered session: " + sanitize_session_id(session_id) + " (total: " + std::to_string(count) + ")");

    return session_id;
  }

  return "";
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
    log_message("[ConnectionManager] Unregistered session: " + sanitize_session_id(session_id) + " (remaining: " +
                std::to_string(count) + ")");
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

std::string connection_manager::generate_session_id() {
  try {
    return generate_crypto_session_id();
  } catch (const std::exception& e) {
    log_error(std::string("[ConnectionManager] Failed to generate session ID: ") + e.what());
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
