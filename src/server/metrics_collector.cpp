#include "metrics_collector.hpp"

namespace cppsim {
namespace server {

metrics_collector::metrics_collector() : start_time_(std::chrono::steady_clock::now()) {}
metrics_collector::~metrics_collector() = default;

// Helper function to calculate percentile
// Precondition: samples must already be sorted in ascending order.
double metrics_collector::calculate_percentile(const std::vector<int64_t>& sorted_samples, int percentile) {
    if (sorted_samples.empty()) return 0.0;

    double index = (percentile / 100.0) * static_cast<double>(sorted_samples.size() - 1);
    size_t lower_idx = static_cast<size_t>(std::floor(index));
    size_t upper_idx = static_cast<size_t>(std::ceil(index));

    if (lower_idx == upper_idx) {
        return static_cast<double>(sorted_samples[lower_idx]);
    }

    double weight = index - static_cast<double>(lower_idx);
    return static_cast<double>(sorted_samples[lower_idx]) * (1 - weight) +
           static_cast<double>(sorted_samples[upper_idx]) * weight;
}

void metrics_collector::increment_counter(const std::string& name, int64_t value) noexcept {
    try {
        auto& m = instance();
        std::lock_guard<std::mutex> lock(m.counters_mutex_);
        m.counters_[name] += value;
    } catch (...) {
    }
}

void metrics_collector::set_gauge(const std::string& name, double value) noexcept {
    try {
        auto& m = instance();
        std::lock_guard<std::mutex> lock(m.gauges_mutex_);
        m.gauges_[name] = value;
    } catch (...) {
    }
}

void metrics_collector::record_timing(const std::string& name, std::chrono::milliseconds duration) noexcept {
    try {
        auto& m = instance();
        std::lock_guard<std::mutex> lock(m.timings_mutex_);
        auto& timing_data = m.timings_[name];

        timing_data.count++;
        timing_data.sum += duration.count();
        timing_data.min = std::min(timing_data.min, duration.count());
        timing_data.max = std::max(timing_data.max, duration.count());

        if (timing_data.samples.size() >= 1000) {
            timing_data.samples.pop_front();
        }
        timing_data.samples.push_back(duration.count());
    } catch (...) {
    }
}

void metrics_collector::record_event(const std::string& name, const std::vector<std::string>& tags) noexcept {
    try {
        auto& m = instance();
        std::lock_guard<std::mutex> lock(m.events_mutex_);

        m.events_.push_back({
            std::chrono::steady_clock::now(),
            name,
            tags
        });

        if (m.events_.size() > 1000) {
            m.events_.pop_front();
        }
    } catch (...) {
    }
}

void metrics_collector::record_error(const std::string& error_type, const std::string& details) noexcept {
    try {
        auto& m = instance();
        {
            std::lock_guard<std::mutex> lock(m.errors_mutex_);

            m.errors_.push_back({
                std::chrono::steady_clock::now(),
                error_type,
                details
            });

            if (m.errors_.size() > 500) {
                m.errors_.pop_front();
            }
        }

        // Increment counters after releasing errors_mutex_ to avoid holding
        // two locks simultaneously (counters_mutex_ + errors_mutex_).
        increment_counter("errors.total");
        increment_counter("errors." + error_type, 1);
    } catch (...) {
    }
}

std::string metrics_collector::export_metrics() noexcept {
    try {
        auto& m = instance();
        std::lock_guard<std::mutex> lock(m.export_mutex_);

        nlohmann::json json_result;

        // Export counters
        json_result["counters"] = nlohmann::json::object();
        {
            std::lock_guard<std::mutex> clk(m.counters_mutex_);
            for (const auto& [name, value] : m.counters_) {
                json_result["counters"][name] = value;
            }
        }

        // Export gauges
        json_result["gauges"] = nlohmann::json::object();
        {
            std::lock_guard<std::mutex> glk(m.gauges_mutex_);
            for (const auto& [name, value] : m.gauges_) {
                json_result["gauges"][name] = value;
            }
        }

        // Export timings
        json_result["timings"] = nlohmann::json::object();
        {
            std::lock_guard<std::mutex> tlk(m.timings_mutex_);
            for (const auto& [name, data] : m.timings_) {
                nlohmann::json timing_json;
                timing_json["count"] = data.count;
                timing_json["sum_ms"] = data.sum;
                timing_json["min_ms"] = data.min;
                timing_json["max_ms"] = data.max;
                timing_json["avg_ms"] = data.count > 0
                    ? static_cast<double>(data.sum) / static_cast<double>(data.count)
                    : 0.0;

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
            std::lock_guard<std::mutex> elk(m.events_mutex_);
            for (const auto& event : m.events_) {
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
            std::lock_guard<std::mutex> erlk(m.errors_mutex_);
            for (const auto& error : m.errors_) {
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
                std::chrono::steady_clock::now() - m.start_time_).count()}
        };

        return json_result.dump(2);

    } catch (...) {
        return "{}";
    }
}

int64_t metrics_collector::get_counter(const std::string& name) noexcept {
    try {
        auto& m = instance();
        std::lock_guard<std::mutex> lock(m.counters_mutex_);
        auto it = m.counters_.find(name);
        return it != m.counters_.end() ? it->second : 0;
    } catch (...) {
        return 0;
    }
}

double metrics_collector::get_gauge(const std::string& name) noexcept {
    try {
        auto& m = instance();
        std::lock_guard<std::mutex> lock(m.gauges_mutex_);
        auto it = m.gauges_.find(name);
        return it != m.gauges_.end() ? it->second : 0.0;
    } catch (...) {
        return 0.0;
    }
}

void metrics_collector::reset() noexcept {
    try {
        auto& m = instance();

        {
            std::lock_guard<std::mutex> clk(m.counters_mutex_);
            m.counters_.clear();
        }
        {
            std::lock_guard<std::mutex> glk(m.gauges_mutex_);
            m.gauges_.clear();
        }
        {
            std::lock_guard<std::mutex> tlk(m.timings_mutex_);
            m.timings_.clear();
        }
        {
            std::lock_guard<std::mutex> elk(m.events_mutex_);
            m.events_.clear();
        }
        {
            std::lock_guard<std::mutex> erlk(m.errors_mutex_);
            m.errors_.clear();
        }

        m.start_time_ = std::chrono::steady_clock::now();

    } catch (...) {
    }
}

std::chrono::steady_clock::time_point metrics_collector::get_start_time() noexcept {
    try {
        return instance().start_time_;
    } catch (...) {
        return std::chrono::steady_clock::now();
    }
}

// Convenience inline functions are defined in the header.

} // namespace server
} // namespace cppsim
