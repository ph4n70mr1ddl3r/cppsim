#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <chrono>
#include <deque>
#include <mutex>
#include <nlohmann/json.hpp>
#include <numeric>
#include <sstream>
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
    // Delete copy constructor and assignment operator
    metrics_collector(const metrics_collector&) = delete;
    metrics_collector& operator=(const metrics_collector&) = delete;
    
    // Allow move semantics
    metrics_collector(metrics_collector&&) = default;
    metrics_collector& operator=(metrics_collector&&) = default;
    
    static metrics_collector& instance() {
        static metrics_collector instance;
        return instance;
    }
    
    /**
     * @brief Increment a counter metric
     * @param name Metric name
     * @param value Value to increment (default: 1)
     */
    static void increment_counter(const std::string& name, int64_t value = 1) noexcept {
        try {
            auto& metrics = instance();
            std::lock_guard<std::mutex> lock(metrics.counters_mutex_);
            metrics.counters_[name] += value;
        } catch (...) {
            // Ignore metric collection errors to avoid impacting server operation
        }
    }
    
    /**
     * @brief Set a gauge metric
     * @param name Metric name
     * @param value Gauge value
     */
    static void set_gauge(const std::string& name, double value) noexcept {
        try {
            auto& metrics = instance();
            std::lock_guard<std::mutex> lock(metrics.gauges_mutex_);
            metrics.gauges_[name] = value;
        } catch (...) {
            // Ignore metric collection errors to avoid impacting server operation
        }
    }
    
    /**
     * @brief Record a timing measurement
     * @param name Metric name
     * @param duration Duration to record
     */
    static void record_timing(const std::string& name, std::chrono::milliseconds duration) noexcept {
        try {
            auto& metrics = instance();
            std::lock_guard<std::mutex> lock(metrics.timings_mutex_);
            auto& timing_data = metrics.timings_[name];
            
            // Update histogram
            timing_data.count++;
            timing_data.sum += duration.count();
            timing_data.min = std::min(timing_data.min, duration.count());
            timing_data.max = std::max(timing_data.max, duration.count());
            
            // Keep only recent samples for performance
            if (timing_data.samples.size() >= 1000) {
                timing_data.samples.pop_front();
            }
            timing_data.samples.push_back(duration.count());
            
        } catch (...) {
            // Ignore metric collection errors to avoid impacting server operation
        }
    }
    
    /**
     * @brief Record an event
     * @param name Event name
     * @param tags Optional event tags
     */
    static void record_event(const std::string& name, const std::vector<std::string>& tags = {}) noexcept {
        try {
            auto& metrics = instance();
            std::lock_guard<std::mutex> lock(metrics.events_mutex_);
            
            metrics.events_.push_back({
                std::chrono::steady_clock::now(),
                name,
                tags
            });
            
            // Keep only recent events
            if (metrics.events_.size() > 1000) {
                metrics.events_.pop_front();
            }
            
        } catch (...) {
            // Ignore metric collection errors to avoid impacting server operation
        }
    }
    
    /**
     * @brief Record an error
     * @param error_type Type of error
     * @param details Error details
     */
    static void record_error(const std::string& error_type, const std::string& details = "") noexcept {
        try {
            auto& metrics = instance();
            std::lock_guard<std::mutex> lock(metrics.errors_mutex_);
            
            metrics.errors_.push_back({
                std::chrono::steady_clock::now(),
                error_type,
                details
            });
            
            // Keep only recent errors
            if (metrics.errors_.size() > 500) {
                metrics.errors_.pop_front();
            }
            
            // Increment error counter
            increment_counter("errors.total");
            increment_counter("errors." + error_type, 1);
            
        } catch (...) {
            // Ignore metric collection errors to avoid impacting server operation
        }
    }
    
    /**
     * @brief Export all metrics as JSON
     * @return JSON string containing all metrics
     */
    static std::string export_metrics() noexcept {
        try {
            auto& metrics = instance();
            std::lock_guard<std::mutex> lock(metrics.export_mutex_);
            
            nlohmann::json json_result;
            
            // Snapshot each data structure under its own lock to avoid torn reads.
            // The export_mutex_ serializes full exports; individual mutexes
            // prevent concurrent mutation while we iterate.
            
            // Export counters
            json_result["counters"] = nlohmann::json::object();
            {
                std::lock_guard<std::mutex> clk(metrics.counters_mutex_);
                for (const auto& [name, value] : metrics.counters_) {
                    json_result["counters"][name] = value;
                }
            }
            
            // Export gauges
            json_result["gauges"] = nlohmann::json::object();
            {
                std::lock_guard<std::mutex> glk(metrics.gauges_mutex_);
                for (const auto& [name, value] : metrics.gauges_) {
                    json_result["gauges"][name] = value;
                }
            }
            
            // Export timings
            json_result["timings"] = nlohmann::json::object();
            {
                std::lock_guard<std::mutex> tlk(metrics.timings_mutex_);
                for (const auto& [name, data] : metrics.timings_) {
                    nlohmann::json timing_json;
                    timing_json["count"] = data.count;
                    timing_json["sum_ms"] = data.sum;
                    timing_json["min_ms"] = data.min;
                    timing_json["max_ms"] = data.max;
                    timing_json["avg_ms"] = data.count > 0 ? static_cast<double>(data.sum) / static_cast<double>(data.count) : 0.0;
                    
                    // Calculate percentiles
                    if (!data.samples.empty()) {
                        std::vector<int64_t> sorted_samples(data.samples.begin(), data.samples.end());
                        std::sort(sorted_samples.begin(), sorted_samples.end());
                        
                        timing_json["p50_ms"] = calculate_percentile(sorted_samples, 50);
                        timing_json["p95_ms"] = calculate_percentile(sorted_samples, 95);
                        timing_json["p99_ms"] = calculate_percentile(sorted_samples, 99);
                    }
                    
                    json_result["timings"][name] = timing_json;
                }
            }
            
            // Export recent events
            json_result["events"] = nlohmann::json::array();
            {
                std::lock_guard<std::mutex> elk(metrics.events_mutex_);
                for (const auto& event : metrics.events_) {
                    nlohmann::json event_json;
                    auto timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                    event_json["timestamp"] = std::to_string(timestamp);
                    event_json["name"] = event.name;
                    event_json["tags"] = event.tags;
                    json_result["events"].push_back(event_json);
                }
            }
            
            // Export recent errors
            json_result["errors"] = nlohmann::json::array();
            {
                std::lock_guard<std::mutex> erlk(metrics.errors_mutex_);
                for (const auto& error : metrics.errors_) {
                    nlohmann::json error_json;
                    auto timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                    error_json["timestamp"] = std::to_string(timestamp);
                    error_json["type"] = error.type;
                    error_json["details"] = error.details;
                    json_result["errors"].push_back(error_json);
                }
            }
            
            // Export system info
            json_result["system"] = {
                {"export_timestamp", std::chrono::system_clock::to_time_t(
                    std::chrono::system_clock::now())},
                {"uptime_seconds", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - metrics.start_time_).count()}
            };
            
            return json_result.dump(2);
            
        } catch (...) {
            return "{}";
        }
    }
    
    /**
     * @brief Get a specific counter value
     * @param name Metric name
     * @return Counter value or 0 if not found
     */
    static int64_t get_counter(const std::string& name) noexcept {
        try {
            auto& metrics = instance();
            std::lock_guard<std::mutex> lock(metrics.counters_mutex_);
            auto it = metrics.counters_.find(name);
            return it != metrics.counters_.end() ? it->second : 0;
        } catch (...) {
            return 0;
        }
    }
    
    /**
     * @brief Get a specific gauge value
     * @param name Metric name
     * @return Gauge value or 0.0 if not found
     */
    static double get_gauge(const std::string& name) noexcept {
        try {
            auto& metrics = instance();
            std::lock_guard<std::mutex> lock(metrics.gauges_mutex_);
            auto it = metrics.gauges_.find(name);
            return it != metrics.gauges_.end() ? it->second : 0.0;
        } catch (...) {
            return 0.0;
        }
    }
    
    /**
     * @brief Reset all metrics
     */
    static void reset() noexcept {
        try {
            auto& metrics = instance();

            // Reset each data structure under its own lock.  No export_mutex_
            // needed here — we're not reading across structures atomically.
            {
                std::lock_guard<std::mutex> clk(metrics.counters_mutex_);
                metrics.counters_.clear();
            }
            {
                std::lock_guard<std::mutex> glk(metrics.gauges_mutex_);
                metrics.gauges_.clear();
            }
            {
                std::lock_guard<std::mutex> tlk(metrics.timings_mutex_);
                metrics.timings_.clear();
            }
            {
                std::lock_guard<std::mutex> elk(metrics.events_mutex_);
                metrics.events_.clear();
            }
            {
                std::lock_guard<std::mutex> erlk(metrics.errors_mutex_);
                metrics.errors_.clear();
            }

            metrics.start_time_ = std::chrono::steady_clock::now();

        } catch (...) {
            // Ignore reset errors
        }
    }
    
    /**
     * @brief Get server start time
     * @return Server start time
     */
    static std::chrono::steady_clock::time_point get_start_time() noexcept {
        try {
            return instance().start_time_;
        } catch (...) {
            return std::chrono::steady_clock::now();
        }
    }

private:
    metrics_collector() : start_time_(std::chrono::steady_clock::now()) {}
    ~metrics_collector() = default;
    
    // Helper function to calculate percentile
    // Precondition: samples must already be sorted in ascending order.
    // The caller (export_metrics) sorts before calling this function.
    static double calculate_percentile(const std::vector<int64_t>& sorted_samples, int percentile) {
        if (sorted_samples.empty()) return 0.0;
        
        double index = (percentile / 100.0) * static_cast<double>(sorted_samples.size() - 1);
        size_t lower_idx = static_cast<size_t>(std::floor(index));
        size_t upper_idx = static_cast<size_t>(std::ceil(index));
        
        if (lower_idx == upper_idx) {
            return static_cast<double>(sorted_samples[lower_idx]);
        }
        
        double weight = index - static_cast<double>(lower_idx);
        return static_cast<double>(sorted_samples[lower_idx]) * (1 - weight) + static_cast<double>(sorted_samples[upper_idx]) * weight;
    }
    
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