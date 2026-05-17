#pragma once

#include "server/boost_wrapper.hpp"
#include <chrono>
#include <cstdint>
#include <random>
#include <thread>

namespace cppsim {
namespace testing {

// Wait for a TCP server to become reachable by polling with probe connections.
// Returns true on success, false on timeout.
inline bool wait_for_server(uint16_t port,
                             std::chrono::steady_clock::duration timeout = std::chrono::seconds(5)) {
  namespace net = boost::asio;
  using tcp = net::ip::tcp;

  auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    try {
      net::io_context ioc;
      tcp::socket s(ioc);
      tcp::resolver resolver(ioc);
      auto const results = resolver.resolve("localhost", std::to_string(port));
      net::connect(s, results.begin(), results.end());
      boost::system::error_code ec;
      s.close(ec);
      return true;
    } catch (...) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
  return false;
}

// Pick a random port in the IANA dynamic range, with optional retry on bind failure.
// Returns 0 if all attempts fail.
template <typename ServerFactory>
uint16_t find_free_port(ServerFactory factory, int max_attempts = 5) {
  std::random_device rd;
  std::uniform_int_distribution<uint16_t> dist(30000, 49999);
  for (int attempt = 0; attempt < max_attempts; ++attempt) {
    uint16_t port = dist(rd);
    try {
      factory(port);
      return port;
    } catch (const std::exception&) {
      // Port in use — try another
    }
  }
  return 0;
}

}  // namespace testing
}  // namespace cppsim
