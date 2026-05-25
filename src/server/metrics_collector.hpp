#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace cppsim {
namespace server {

/**
 * @brief High-performance metrics collection for poker server
 *
 * Provides thread-safe metrics collection with minimal overhead.
 * Supports counters, gauges, histograms, and timing metrics.
 * Designed to be called frequently without impacting performance.
 */
class metrics_collector {
public:
    metrics_collector(const metrics_collector&) = delete;
    metrics_collector& operator=(const metrics_collector&) = delete;
    metrics_collector(metrics_collector&&) = delete;
    metrics_collector& operator=(metrics_collector&&) = delete;

    static metrics_collector& instance() {
        static metrics_collector instance;
        return instance;
    }

    static void increment_counter(const std::string& name, int64_t value = 1) noexcept;
    static void set_gauge(const std::string& name, double value) noexcept;
    static void record_timing(const std::string& name, std::chrono::milliseconds duration) noexcept;
    static void record_event(const std::string& name, const std::vector<std::string>& tags = {}) noexcept;
    static void record_error(const std::string& error_type, const std::string& details = "") noexcept;
    static std::string export_metrics() noexcept;
    static int64_t get_counter(const std::string& name) noexcept;
    static double get_gauge(const std::string& name) noexcept;
    static void reset() noexcept;
    static std::chrono::steady_clock::time_point get_start_time() noexcept;

private:
    metrics_collector();
    ~metrics_collector();

    static double calculate_percentile(const std::vector<int64_t>& sorted_samples, int percentile);

    // Counter metrics
    std::unordered_map<std::string, int64_t> counters_;
    mutable std::mutex counters_mutex_;

    // Gauge metrics
    std::unordered_map<std::string, double> gauges_;
    mutable std::mutex gauges_mutex_;

    // Timing metrics
    struct timing_data {
        int64_t count{0};
        int64_t sum{0};
        int64_t min{std::numeric_limits<int64_t>::max()};
        int64_t max{0};
        std::deque<int64_t> samples;
    };
    std::unordered_map<std::string, timing_data> timings_;
    mutable std::mutex timings_mutex_;

    // Event metrics
    struct event_data {
        std::chrono::steady_clock::time_point timestamp;
        std::string name;
        std::vector<std::string> tags;
    };
    std::deque<event_data> events_;
    mutable std::mutex events_mutex_;

    // Error metrics
    struct error_data {
        std::chrono::steady_clock::time_point timestamp;
        std::string type;
        std::string details;
    };
    std::deque<error_data> errors_;
    mutable std::mutex errors_mutex_;

    // Export mutex
    mutable std::mutex export_mutex_;

    // System metrics
    std::chrono::steady_clock::time_point start_time_;
};

// Convenience inline functions for common metrics (preferred over macros for
// type safety and standard compliance).
inline void MetricsInc(std::string_view name) { metrics_collector::increment_counter(std::string(name)); }
inline void MetricsIncValue(std::string_view name, int64_t value) { metrics_collector::increment_counter(std::string(name), value); }
inline void MetricsGauge(std::string_view name, double value) { metrics_collector::set_gauge(std::string(name), value); }
inline void MetricsTiming(std::string_view name, std::chrono::milliseconds duration) { metrics_collector::record_timing(std::string(name), duration); }
inline void MetricsEvent(std::string_view name, const std::vector<std::string>& tags = {}) { metrics_collector::record_event(std::string(name), tags); }
inline void MetricsError(std::string_view type, std::string_view details = "") { metrics_collector::record_error(std::string(type), std::string(details)); }

} // namespace server
} // namespace cppsim
