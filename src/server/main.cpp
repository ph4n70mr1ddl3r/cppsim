#include <csignal>
#include <cstdlib>
#include <thread>
#include <atomic>

#include "boost_wrapper.hpp"
#include "config.hpp"
#include "logger.hpp"
#include "protocol.hpp"
#include "websocket_server.hpp"
#include "runtime_config_manager.hpp"
#include "metrics_collector.hpp"
#include <filesystem>

int main() {
  try {
    // Set up error logging
    cppsim::protocol::set_error_logger([](std::string_view msg) {
      cppsim::server::log_error(msg);
      cppsim::server::metrics_collector::record_error("protocol_error", std::string(msg));
    });
    
    // Initialize metrics collection
    cppsim::server::metrics_collector::record_event("server_start");
    
    // Load configuration
    auto& config = cppsim::server::runtime_config_manager::instance();
    std::string config_path = "config/default_config.json";
    
    // Check if config file exists, if not create it
    if (!std::filesystem::exists(config_path)) {
      cppsim::server::log_message("[Main] Config file not found, using defaults");
    } else {
      if (!config.load_from_file(config_path)) {
        cppsim::server::log_message("[Main] Failed to load config, using defaults");
      } else {
        cppsim::server::log_message("[Main] Configuration loaded successfully");
      }
    }
    
    // Log configuration summary
    cppsim::server::log_message("[Main] Server configuration:");
    cppsim::server::log_message("  - Max connections: " + std::to_string(config.get_max_connections()));
    cppsim::server::log_message("  - Handshake timeout: " + std::to_string(config.get_handshake_timeout().count()) + "s");
    cppsim::server::log_message("  - Max message size: " + std::to_string(config.get_max_message_size()));
    cppsim::server::log_message("  - Security enabled: " + std::string(config.is_security_enabled() ? "true" : "false"));
    cppsim::server::log_message("  - Metrics enabled: " + std::string(config.is_metrics_enabled() ? "true" : "false"));
    
    boost::asio::io_context ioc;
    std::atomic<bool> running{true};

    // Create server with configuration-based port
    auto server = std::make_shared<cppsim::server::websocket_server>(ioc, cppsim::server::config::DEFAULT_PORT);

    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&running, &ioc](boost::beast::error_code const&, int) {
      cppsim::server::log_message("[Main] Shutting down server...");
      
      // Signal metrics thread to stop
      running = false;
      
      // Record shutdown metrics
      auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::steady_clock::now() - cppsim::server::metrics_collector::instance().get_start_time()).count();
      cppsim::server::metrics_collector::record_event("server_shutdown");
      cppsim::server::metrics_collector::set_gauge("server_uptime_seconds", static_cast<double>(uptime));
      
      ioc.stop();
    });

    cppsim::server::log_message("[Main] Server running. Press Ctrl+C to stop.");
    
    // Start metrics collection thread if enabled
    std::thread metrics_thread;
    if (config.is_metrics_enabled()) {
      metrics_thread = std::thread([&running]() {
        while (running) {
          // Sleep in short increments so the thread exits promptly when
          // `running` becomes false during shutdown.
          for (int i = 0; i < 30 && running; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
          }
          if (!running) break;
          try {
            std::string metrics = cppsim::server::metrics_collector::export_metrics();
            cppsim::server::log_message("[Metrics] Exported " + std::to_string(metrics.size()) + " bytes of metrics");
          } catch (const std::exception& e) {
            cppsim::server::log_error(std::string("[Metrics] Export failed: ") + e.what());
          }
        }
      });
    }
    
    server->run();
    ioc.run();

    // Join metrics thread before destroying shared state (the `running` flag,
    // metrics_collector singleton, etc.).  A detached thread could outlive
    // main() and touch destroyed objects during shutdown.
    if (metrics_thread.joinable()) {
      metrics_thread.join();
    }

    cppsim::server::log_message("[Main] Server stopped.");
    return EXIT_SUCCESS;

  } catch (const std::exception& e) {
    cppsim::server::log_error(std::string("[Main] Exception: ") + e.what());
    cppsim::server::metrics_collector::record_error("startup_error", e.what());
    return EXIT_FAILURE;
  } catch (...) {
    cppsim::server::log_error("[Main] Unknown exception occurred");
    cppsim::server::metrics_collector::record_error("startup_error", "Unknown exception");
    return EXIT_FAILURE;
  }
}
