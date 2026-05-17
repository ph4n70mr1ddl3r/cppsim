#include "connection_manager.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string_view>
#include <thread>

#include "config.hpp"
#include "logger.hpp"
#include "sanitize.hpp"
#include "websocket_session.hpp"

namespace {

using cppsim::server::sanitize_session_id;

// Cryptographic constants
constexpr size_t SESSION_ID_ENTROPY_BYTES = 16;  // 128 bits of entropy
constexpr size_t HEX_CHARS_PER_BYTE = 2;
constexpr size_t HEX_BUFFER_SIZE = SESSION_ID_ENTROPY_BYTES * HEX_CHARS_PER_BYTE + 1;  // +1 for null terminator
constexpr size_t RAND_S_BYTES_PER_CALL = 4;
constexpr int MAX_SESSION_ID_RETRIES = 3;
constexpr uint64_t MURMUR_HASH_CONSTANT1 = 0xff51afd7ed558ccdULL;
constexpr uint64_t MURMUR_HASH_CONSTANT2 = 0xc4ceb9fe1a85ec53ULL;

// Generate cryptographically secure random bytes from /dev/urandom (Linux) or arc4random (BSD/macOS).
// Falls back to a hash-mixed entropy source if OS CPRNG is unavailable.
bool get_secure_random(unsigned char* buf, size_t len) noexcept {
#if defined(__linux__)
  std::FILE* f = std::fopen("/dev/urandom", "rb");
  if (!f) {
    return false;
  }
  struct file_closer {
    std::FILE* fp;
    explicit file_closer(std::FILE* file) : fp(file) {}
    ~file_closer() { if (fp) std::fclose(fp); }
    // Prevent copying
    file_closer(const file_closer&) = delete;
    file_closer& operator=(const file_closer&) = delete;
  } closer{f};
  size_t read = std::fread(buf, 1, len, f);
  return read == len;
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
  arc4random_buf(buf, len);
  return true;
#elif defined(_WIN32)
  // Use rand_s() which calls RtlGenRandom (aka SystemFunction036) —
  // a user-mode CSPRNG backed by the Windows kernel. Available since
  // MSVC 2005 on all supported Windows versions.
  size_t i = 0;
  while (i < len) {
    unsigned int val = 0;
    if (rand_s(&val) != 0) {
      return false;
    }
    // Use all bytes from each rand_s() call
    unsigned char bytes[RAND_S_BYTES_PER_CALL] = {
      static_cast<unsigned char>(val & 0xFF),
      static_cast<unsigned char>((val >> 8) & 0xFF),
      static_cast<unsigned char>((val >> 16) & 0xFF),
      static_cast<unsigned char>((val >> 24) & 0xFF)
    };
    size_t to_copy = std::min(len - i, static_cast<size_t>(RAND_S_BYTES_PER_CALL));
    std::memcpy(buf + i, bytes, to_copy);
    i += to_copy;
  }
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
  h *= MURMUR_HASH_CONSTANT1;
  h ^= h >> 33;
  h *= MURMUR_HASH_CONSTANT2;
  h ^= h >> 33;

  std::array<char, HEX_BUFFER_SIZE> buf;  // 16 hex chars + null terminator
  std::snprintf(buf.data(), buf.size(), "%016" PRIx64, h);
  return "sess_" + std::string(buf.data());
}

// Generate a cryptographically random session ID: sess_{32 hex chars} = 128 bits of entropy.
std::string generate_crypto_session_id() {
  std::array<unsigned char, SESSION_ID_ENTROPY_BYTES> random_bytes{};  // 128 bits

  if (!get_secure_random(random_bytes.data(), random_bytes.size())) {
    cppsim::server::log_error("[ConnectionManager] CSPRNG unavailable, using fallback session ID");
    return generate_fallback_session_id();
  }

  std::array<char, HEX_BUFFER_SIZE> hex;
  static constexpr char hex_chars[] = "0123456789abcdef";
  for (size_t i = 0; i < random_bytes.size(); ++i) {
    hex[i * HEX_CHARS_PER_BYTE] = hex_chars[(random_bytes[i] >> 4) & 0x0f];
    hex[i * HEX_CHARS_PER_BYTE + 1] = hex_chars[random_bytes[i] & 0x0f];
  }
  hex[SESSION_ID_ENTROPY_BYTES * HEX_CHARS_PER_BYTE] = '\0';

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
  
  for (int attempt = 0; attempt < MAX_SESSION_ID_RETRIES; ++attempt) {
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
      // session is a shared_ptr copy — try_emplace copies it into the map,
      // so the caller always retains their reference regardless of attempt count.
      auto result = sessions_.try_emplace(session_id, session);
      if (!result.second) {
        log_error("[ConnectionManager] Session ID collision (attempt " +
            std::to_string(attempt + 1) + "), retrying: " + sanitize_session_id(session_id));
        continue;
      }
      count = sessions_.size();
    }

    log_message("[ConnectionManager] Registered session: " + sanitize_session_id(session_id) + " (total: " + std::to_string(count) + ")");

    return session_id;
  }

  log_error("[ConnectionManager] Session ID collision after all retries");
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

std::string connection_manager::generate_session_id() noexcept {
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
