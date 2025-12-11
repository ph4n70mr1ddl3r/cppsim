#include <iostream>
#include "boost_wrapper.hpp"
#include <csignal>

#include "websocket_server.hpp"

int main() {
  try {
    // Create the io_context
    boost::asio::io_context ioc;

    // Create the WebSocket server on port 8080
    constexpr uint16_t port = 8080;
    cppsim::server::websocket_server server(ioc, port);

    // Set up signal handling for graceful shutdown
    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&](boost::beast::error_code const&, int) {
      std::cout << "\n[Main] Shutting down server..." << std::endl;
      server.stop();
      ioc.stop();
    });

    // Start the server
    server.run();

    std::cout << "[Main] Server running. Press Ctrl+C to stop." << std::endl;

    // Run the io_context (blocks until stopped)
    ioc.run();

    std::cout << "[Main] Server stopped." << std::endl;
    return 0;

  } catch (const std::exception& e) {
    std::cerr << "[Main] Exception: " << e.what() << std::endl;
    return 1;
  }
}
