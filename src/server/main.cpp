#include <cstdlib>

#include "boost_wrapper.hpp"
#include <csignal>

#include "config.hpp"
#include "logger.hpp"
#include "protocol.hpp"
#include "websocket_server.hpp"

int main() {
  try {
    cppsim::protocol::set_error_logger([](std::string_view msg) {
      cppsim::server::log_error(msg);
    });

    boost::asio::io_context ioc;

    auto server = std::make_shared<cppsim::server::websocket_server>(ioc, cppsim::server::config::DEFAULT_PORT);

    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([server, &ioc](boost::beast::error_code const&, int) {
      cppsim::server::log_message("[Main] Shutting down server...");
      server->stop();
      ioc.stop();
    });

    cppsim::server::log_message("[Main] Server running. Press Ctrl+C to stop.");
    server->run();

    ioc.run();

    cppsim::server::log_message("[Main] Server stopped.");
    return EXIT_SUCCESS;

  } catch (const std::exception& e) {
    cppsim::server::log_error(std::string("[Main] Exception: ") + e.what());
    return EXIT_FAILURE;
  }
}
