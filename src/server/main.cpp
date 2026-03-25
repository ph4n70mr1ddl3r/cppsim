#include <cstdlib>

#include "boost_wrapper.hpp"
#include <csignal>

#include "config.hpp"
#include "logger.hpp"
#include "websocket_server.hpp"

int main() {
  try {
    boost::asio::io_context ioc;

    cppsim::server::websocket_server server(ioc, cppsim::server::config::DEFAULT_PORT);

    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&](boost::beast::error_code const&, int) {
      cppsim::server::log_message("[Main] Shutting down server...");
      server.stop();
      ioc.stop();
    });

    cppsim::server::log_message("[Main] Server running. Press Ctrl+C to stop.");
    server.run();

    ioc.run();

    cppsim::server::log_message("[Main] Server stopped.");
    return EXIT_SUCCESS;

  } catch (const std::exception& e) {
    cppsim::server::log_error(std::string("[Main] Exception: ") + e.what());
    return EXIT_FAILURE;
  }
}
