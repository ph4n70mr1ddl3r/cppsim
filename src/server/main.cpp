#include <iostream>
#include "boost_wrapper.hpp"
#include <csignal>
#include <cstdlib>

#include "websocket_server.hpp"
#include "config.hpp"

int main() {
  try {
    // Create the io_context
    boost::asio::io_context ioc;

    // Create the WebSocket server
    cppsim::server::websocket_server server(ioc, cppsim::server::config::DEFAULT_PORT);

    // Set up signal handling for graceful shutdown
    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&](boost::beast::error_code const&, int) {
      std::cout << "\n[Main] Shutting down server...\n";
      server.stop();
      ioc.stop();
    });

    // Start the server
    std::cout << "[Main] Server running. Press Ctrl+C to stop.\n";
    server.run();

    // Run the io_context (blocks until stopped)
    ioc.run();

    std::cout << "[Main] Server stopped.\n";
    return EXIT_SUCCESS;

  } catch (const std::exception& e) {
    std::cerr << "[Main] Exception: " << e.what() << '\n';
    return EXIT_FAILURE;
  }
}
