#include "server/runtime_config_manager.hpp"
#include "server/metrics_collector.hpp"

#include <gtest/gtest.h>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <thread>

using namespace cppsim::server;

class ConfigManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary config file for testing
        temp_dir_ = std::filesystem::temp_directory_path() / "cppsim_test";
        std::filesystem::create_directories(temp_dir_);
        config_file_ = temp_dir_ / "test_config.json";
        
        // Create default test config
        nlohmann::json test_config = {
            {"max_connections", 500},
            {"handshake_timeout", 15},
            {"max_message_size", 32768},
            {"max_write_queue_size", 50},
            {"max_messages_per_window", 5},
            {"rate_limit_window", 2},
            {"max_backoff", 60},
            {"security_enabled", true},
            {"metrics_enabled", true}
        };
        
        std::ofstream config_file(config_file_);
        config_file << test_config.dump(2);
        config_file.close();
    }
    
    void TearDown() override {
        // Clean up temporary files
        std::filesystem::remove_all(temp_dir_);
    }
    
    std::filesystem::path temp_dir_;
    std::filesystem::path config_file_;
};

TEST_F(ConfigManagerTest, LoadValidConfig) {
    auto& config = runtime_config_manager::instance();
    
    // Load the test config
    ASSERT_TRUE(config.load_from_file(config_file_.string()));
    
    // Verify loaded values
    EXPECT_EQ(config.get_max_connections(), 500);
    EXPECT_EQ(config.get_handshake_timeout().count(), 15);
    EXPECT_EQ(config.get_max_message_size(), 32768);
    EXPECT_EQ(config.get_max_write_queue_size(), 50);
    EXPECT_EQ(config.get_max_messages_per_window(), 5);
    EXPECT_EQ(config.get_rate_limit_window().count(), 2);
    EXPECT_EQ(config.get_max_backoff().count(), 60);
    EXPECT_TRUE(config.is_security_enabled());
    EXPECT_TRUE(config.is_metrics_enabled());
}

TEST_F(ConfigManagerTest, LoadInvalidConfigValues) {
    auto& config = runtime_config_manager::instance();
    
    // Create config with invalid values
    nlohmann::json invalid_config = {
        {"max_connections", 0},  // Invalid
        {"handshake_timeout", 0},  // Invalid
        {"max_message_size", 100},  // Too small
        {"max_write_queue_size", 5},  // Less than max_messages_per_window
        {"security_enabled", "invalid"}  // Wrong type
    };
    
    std::ofstream config_file(config_file_);
    config_file << invalid_config.dump(2);
    config_file.close();
    
    // Should load but use defaults for invalid values
    ASSERT_TRUE(config.load_from_file(config_file_.string()));
    
    // Should use defaults for invalid values
    EXPECT_EQ(config.get_max_connections(), 1000);  // Default
    EXPECT_EQ(config.get_handshake_timeout().count(), 10);  // Default
    EXPECT_EQ(config.get_max_message_size(), 65536);  // Default
    EXPECT_GT(config.get_max_write_queue_size(), 5);  // Should be >= max_messages_per_window
}

TEST_F(ConfigManagerTest, ExportConfigToJson) {
    auto& config = runtime_config_manager::instance();
    
    ASSERT_TRUE(config.load_from_file(config_file_.string()));
    
    std::string json_export = config.export_to_json();
    EXPECT_FALSE(json_export.empty());
    
    // Verify JSON is valid
    nlohmann::json exported_json = nlohmann::json::parse(json_export);
    EXPECT_TRUE(exported_json.contains("max_connections"));
    EXPECT_TRUE(exported_json.contains("handshake_timeout"));
    EXPECT_TRUE(exported_json.contains("security_enabled"));
}

TEST_F(ConfigManagerTest, ReloadConfig) {
    auto& config = runtime_config_manager::instance();

    // First load the initial config
    ASSERT_TRUE(config.load_from_file(config_file_.string()));
    EXPECT_EQ(config.get_max_connections(), 500);

    // Update config file with new values
    nlohmann::json updated_config = {
        {"max_connections", 750},
        {"handshake_timeout", 20},
        {"security_enabled", false}
    };

    std::ofstream config_file(config_file_);
    config_file << updated_config.dump(2);
    config_file.close();

    // Reload config (bypasses throttle since enough time passes in test)
    ASSERT_TRUE(config.load_from_file(config_file_.string()));

    // Verify updated values
    EXPECT_EQ(config.get_max_connections(), 750);
    EXPECT_EQ(config.get_handshake_timeout().count(), 20);
    EXPECT_FALSE(config.is_security_enabled());
}

TEST_F(ConfigManagerTest, MissingConfigFile) {
    // Use a fresh instance check — singleton retains state from prior tests,
    // so we verify the return value rather than checking specific defaults.
    auto& config = runtime_config_manager::instance();

    // Try to load non-existent file
    ASSERT_FALSE(config.load_from_file("/nonexistent/path/config.json"));
}

TEST_F(ConfigManagerTest, MalformedJsonConfig) {
    auto& config = runtime_config_manager::instance();
    
    // Create malformed JSON file
    std::ofstream config_file(config_file_);
    config_file << "{ invalid json content }";
    config_file.close();
    
    // Should fail to load
    ASSERT_FALSE(config.load_from_file(config_file_.string()));
}

TEST_F(ConfigManagerTest, ConfigValidation) {
    auto& config = runtime_config_manager::instance();

    // Test valid config
    nlohmann::json valid_config = {
        {"max_connections", 1000},
        {"handshake_timeout", 10},
        {"max_message_size", 65536},
        {"max_write_queue_size", 100},
        {"max_messages_per_window", 10},
        {"security_enabled", true}
    };

    std::ofstream config_file(config_file_);
    config_file << valid_config.dump(2);
    config_file.close();

    ASSERT_TRUE(config.load_from_file(config_file_.string()));

    // Test invalid relationships: max_write_queue_size < max_messages_per_window
    // Note: max_write_queue_size=5 is clamped to default 100 in load_from_json,
    // so validation passes. Create a config where both are explicit and
    // write_queue < messages_per_window after clamping.
    nlohmann::json invalid_config = {
        {"max_write_queue_size", 10},    // Valid range, not clamped
        {"max_messages_per_window", 20} // Valid range, not clamped
    };

    std::ofstream config_file2(config_file_);
    config_file2 << invalid_config.dump(2);
    config_file2.close();

    // Should fail to load due to invalid relationship
    ASSERT_FALSE(config.load_from_file(config_file_.string()));
}

// Metrics Collector Tests
class MetricsCollectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        metrics_collector::reset();
    }
    
    void TearDown() override {
        metrics_collector::reset();
    }
};

TEST_F(MetricsCollectorTest, CounterMetrics) {
    // Test counter increment
    metrics_collector::increment_counter("test_counter");
    EXPECT_EQ(metrics_collector::get_counter("test_counter"), 1);
    
    // Test counter increment with value
    metrics_collector::increment_counter("test_counter", 5);
    EXPECT_EQ(metrics_collector::get_counter("test_counter"), 6);
    
    // Test counter with different name
    metrics_collector::increment_counter("another_counter", 10);
    EXPECT_EQ(metrics_collector::get_counter("another_counter"), 10);
    
    // Test non-existent counter
    EXPECT_EQ(metrics_collector::get_counter("non_existent"), 0);
}

TEST_F(MetricsCollectorTest, GaugeMetrics) {
    // Test gauge setting
    metrics_collector::set_gauge("test_gauge", 42.5);
    EXPECT_DOUBLE_EQ(metrics_collector::get_gauge("test_gauge"), 42.5);
    
    // Test gauge update
    metrics_collector::set_gauge("test_gauge", 100.0);
    EXPECT_DOUBLE_EQ(metrics_collector::get_gauge("test_gauge"), 100.0);
    
    // Test non-existent gauge
    EXPECT_DOUBLE_EQ(metrics_collector::get_gauge("non_existent"), 0.0);
}

TEST_F(MetricsCollectorTest, TimingMetrics) {
    // Test timing recording
    auto duration = std::chrono::milliseconds(150);
    metrics_collector::record_timing("test_timing", duration);
    
    // Verify metrics are collected (we can't easily test the exact values
    // since they're internal, but we can verify the export works)
    std::string json_export = metrics_collector::export_metrics();
    EXPECT_FALSE(json_export.empty());
    
    nlohmann::json exported_json = nlohmann::json::parse(json_export);
    EXPECT_TRUE(exported_json.contains("timings"));
    EXPECT_TRUE(exported_json["timings"].contains("test_timing"));
}

TEST_F(MetricsCollectorTest, EventMetrics) {
    // Test event recording
    std::vector<std::string> tags = {"tag1", "tag2"};
    metrics_collector::record_event("test_event", tags);
    
    std::string json_export = metrics_collector::export_metrics();
    EXPECT_FALSE(json_export.empty());
    
    nlohmann::json exported_json = nlohmann::json::parse(json_export);
    EXPECT_TRUE(exported_json.contains("events"));
    EXPECT_TRUE(exported_json["events"].is_array());
    EXPECT_GT(exported_json["events"].size(), 0);
}

TEST_F(MetricsCollectorTest, ErrorMetrics) {
    // Test error recording
    metrics_collector::record_error("connection_error", "Failed to connect");
    metrics_collector::record_error("protocol_error", "Invalid message format");
    
    // Verify error counters
    EXPECT_EQ(metrics_collector::get_counter("errors.total"), 2);
    EXPECT_EQ(metrics_collector::get_counter("errors.connection_error"), 1);
    EXPECT_EQ(metrics_collector::get_counter("errors.protocol_error"), 1);
    
    std::string json_export = metrics_collector::export_metrics();
    EXPECT_FALSE(json_export.empty());
    
    nlohmann::json exported_json = nlohmann::json::parse(json_export);
    EXPECT_TRUE(exported_json.contains("errors"));
    EXPECT_TRUE(exported_json["errors"].is_array());
    EXPECT_GT(exported_json["errors"].size(), 0);
}

TEST_F(MetricsCollectorTest, ExportMetrics) {
    // Record some sample metrics
    metrics_collector::increment_counter("test_counter", 10);
    metrics_collector::set_gauge("test_gauge", 25.5);
    metrics_collector::record_timing("test_timing", std::chrono::milliseconds(100));
    metrics_collector::record_event("test_event");
    metrics_collector::record_error("test_error", "Test error message");
    
    // Export metrics
    std::string json_export = metrics_collector::export_metrics();
    EXPECT_FALSE(json_export.empty());
    
    // Verify JSON structure
    nlohmann::json exported_json = nlohmann::json::parse(json_export);
    EXPECT_TRUE(exported_json.contains("counters"));
    EXPECT_TRUE(exported_json.contains("gauges"));
    EXPECT_TRUE(exported_json.contains("timings"));
    EXPECT_TRUE(exported_json.contains("events"));
    EXPECT_TRUE(exported_json.contains("errors"));
    EXPECT_TRUE(exported_json.contains("system"));
    
    // Verify counter values
    EXPECT_EQ(exported_json["counters"]["test_counter"], 10);
    EXPECT_DOUBLE_EQ(exported_json["gauges"]["test_gauge"], 25.5);
}

TEST_F(MetricsCollectorTest, ResetMetrics) {
    // Record some metrics
    metrics_collector::increment_counter("test_counter", 100);
    metrics_collector::set_gauge("test_gauge", 50.0);
    metrics_collector::record_timing("test_timing", std::chrono::milliseconds(200));

    // Verify metrics are present
    EXPECT_GT(metrics_collector::get_counter("test_counter"), 0);
    EXPECT_DOUBLE_EQ(metrics_collector::get_gauge("test_gauge"), 50.0);

    // Reset metrics
    metrics_collector::reset();

    // Verify metrics are cleared via getter API
    EXPECT_EQ(metrics_collector::get_counter("test_counter"), 0);
    EXPECT_DOUBLE_EQ(metrics_collector::get_gauge("test_gauge"), 0.0);

    // Verify exported JSON reflects the reset
    std::string json_export = metrics_collector::export_metrics();
    nlohmann::json exported_json = nlohmann::json::parse(json_export);
    // After reset, counters map is empty so the key won't exist in JSON
    EXPECT_FALSE(exported_json["counters"].contains("test_counter"));
    EXPECT_FALSE(exported_json["gauges"].contains("test_gauge"));
}

TEST_F(MetricsCollectorTest, ThreadSafety) {
    // Test thread safety by recording metrics from multiple threads
    
    const int num_threads = 10;
    const int operations_per_thread = 100;
    std::vector<std::thread> threads;
    
    auto worker_func = [&]() {
        for (int i = 0; i < operations_per_thread; ++i) {
            metrics_collector::increment_counter("thread_counter");
            metrics_collector::set_gauge("thread_gauge", static_cast<double>(i));
            metrics_collector::record_timing("thread_timing", std::chrono::milliseconds(i));
        }
    };
    
    // Start multiple threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker_func);
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify metrics are consistent
    EXPECT_EQ(metrics_collector::get_counter("thread_counter"), 
              num_threads * operations_per_thread);
    
    std::string json_export = metrics_collector::export_metrics();
    EXPECT_FALSE(json_export.empty());
}