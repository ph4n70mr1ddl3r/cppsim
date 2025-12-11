#include <iostream>
#include "boost_wrapper.hpp"
#include <csignal>
#include <cstdlib>

#include "websocket_server.hpp"

// Configuration constants
constexpr uint16_t DEFAULT_PORT = 8080;

int main() {
  try {
    // Create the io_context
    boost::asio::io_context ioc;

    // Create the WebSocket server
    cppsim::server::websocket_server server(ioc, DEFAULT_PORT);

    // Set up signal handling for graceful shutdown
    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&](boost::beast::error_code const&, int) {
      std::cout << "\n[Main] Shutting down server..." << std::endl;
      server.stop();
      ioc.stop();
    });

    // Start the server
    server.run();

    std::cout << "Hello from poker server!" << std::endl;
    std::cout << "[Main] Server running. Press Ctrl+C to stop." << std::endl;

    // Run the io_context (blocks until stopped)
    ioc.run();

    std::cout << "[Main] Server stopped." << std::endl;
    return EXIT_SUCCESS;

  } catch (const std::exception& e) {
    std::cerr << "[Main] Exception: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
