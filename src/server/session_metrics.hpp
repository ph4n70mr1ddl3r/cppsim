#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace cppsim {
namespace server {

/**
 * @brief Metrics collection for WebSocket sessions
 * 
 * Thread-safe metrics collection for monitoring session performance,
 * error rates, and activity patterns. Provides atomic counters and
 * time-based measurements.
 */
class session_metrics {
public:
    session_metrics() = default;
    ~session_metrics() = default;
    
    // Delete copy constructor and assignment operator
    session_metrics(const session_metrics&) = delete;
    session_metrics& operator=(const session_metrics&) = delete;
    
    // std::atomic members are not move-constructible — delete move semantics.
    session_metrics(session_metrics&&) = delete;
    session_metrics& operator=(session_metrics&&) = delete;
    
    /**
     * @brief Increment message counter
     * @param count Number of messages to increment by
     */
    void increment_messages_sent(size_t count = 1) noexcept {
        messages_sent_.fetch_add(count, std::memory_order_relaxed);
    }
    
    /**
     * @brief Increment message reception counter
     * @param count Number of messages to increment by
     */
    void increment_messages_received(size_t count = 1) noexcept {
        messages_received_.fetch_add(count, std::memory_order_relaxed);
    }
    
    /**
     * @brief Increment error counter
     * @param count Number of errors to increment by
     */
    void increment_errors(size_t count = 1) noexcept {
        errors_.fetch_add(count, std::memory_order_relaxed);
    }
    
    /**
     * @brief Record processing time
     * @param time Processing time to record
     */
    void update_processing_time(std::chrono::microseconds time) noexcept {
        total_processing_time_.fetch_add(static_cast<uint64_t>(time.count()), std::memory_order_relaxed);
        processing_time_count_.fetch_add(1, std::memory_order_relaxed);
        
        // Update max processing time
        uint64_t current_max = max_processing_time_.load(std::memory_order_relaxed);
        while (static_cast<uint64_t>(time.count()) > current_max) {
            if (max_processing_time_.compare_exchange_weak(
                    current_max, 
                    static_cast<uint64_t>(time.count()),
                    std::memory_order_relaxed,
                    std::memory_order_relaxed)) {
                break;
            }
        }
    }
    
    /**
     * @brief Record connection duration
     * @param duration Connection duration
     */
    void update_connection_duration(std::chrono::seconds duration) noexcept {
        total_connection_time_.fetch_add(static_cast<uint64_t>(duration.count()), std::memory_order_relaxed);
        connection_count_.fetch_add(1, std::memory_order_relaxed);
        
        // Update max connection time
        uint64_t current_max = max_connection_time_.load(std::memory_order_relaxed);
        while (static_cast<uint64_t>(duration.count()) > current_max) {
            if (max_connection_time_.compare_exchange_weak(
                    current_max, 
                    static_cast<uint64_t>(duration.count()),
                    std::memory_order_relaxed,
                    std::memory_order_relaxed)) {
                break;
            }
        }
    }
    
    /**
     * @brief Record rate limit event
     */
    void record_rate_limit_exceeded() noexcept {
        rate_limit_exceeded_.fetch_add(1, std::memory_order_relaxed);
    }
    
    /**
     * @brief Get total messages sent
     * @return Total number of messages sent
     */
    uint64_t get_messages_sent() const noexcept {
        return messages_sent_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get total messages received
     * @return Total number of messages received
     */
    uint64_t get_messages_received() const noexcept {
        return messages_received_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get total errors
     * @return Total number of errors
     */
    uint64_t get_errors() const noexcept {
        return errors_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get average processing time in milliseconds
     * @return Average processing time, or 0 if no measurements
     */
    double get_average_processing_time_ms() const noexcept {
        uint64_t count = processing_time_count_.load(std::memory_order_relaxed);
        if (count == 0) return 0.0;
        
        uint64_t total = total_processing_time_.load(std::memory_order_relaxed);
        return static_cast<double>(total) / static_cast<double>(count) / 1000.0; // Convert microseconds to milliseconds
    }
    
    /**
     * @brief Get maximum processing time in milliseconds
     * @return Maximum processing time observed
     */
    double get_max_processing_time_ms() const noexcept {
        return static_cast<double>(max_processing_time_.load(std::memory_order_relaxed)) / 1000.0;
    }
    
    /**
     * @brief Get average connection duration in seconds
     * @return Average connection duration, or 0 if no measurements
     */
    double get_average_connection_duration_s() const noexcept {
        uint64_t count = connection_count_.load(std::memory_order_relaxed);
        if (count == 0) return 0.0;
        
        uint64_t total = total_connection_time_.load(std::memory_order_relaxed);
        return static_cast<double>(total) / static_cast<double>(count);
    }
    
    /**
     * @brief Get maximum connection duration in seconds
     * @return Maximum connection duration observed
     */
    double get_max_connection_duration_s() const noexcept {
        return static_cast<double>(max_connection_time_.load(std::memory_order_relaxed));
    }
    
    /**
     * @brief Get rate limit exceed count
     * @return Number of rate limit exceeded events
     */
    uint64_t get_rate_limit_exceeded_count() const noexcept {
        return rate_limit_exceeded_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Increment bytes sent counter
     * @param bytes Number of bytes to increment by
     */
    void increment_bytes_sent(size_t bytes) noexcept {
        bytes_sent_.fetch_add(bytes, std::memory_order_relaxed);
    }
    
    /**
     * @brief Increment bytes received counter
     * @param bytes Number of bytes to increment by
     */
    void increment_bytes_received(size_t bytes) noexcept {
        bytes_received_.fetch_add(bytes, std::memory_order_relaxed);
    }
    
    /**
     * @brief Get total bytes sent
     * @return Total number of bytes sent
     */
    uint64_t get_bytes_sent() const noexcept {
        return bytes_sent_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get total bytes received
     * @return Total number of bytes received
     */
    uint64_t get_bytes_received() const noexcept {
        return bytes_received_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get throughput metrics
     * @return String with formatted throughput information
     */
    std::string get_throughput_info() const noexcept {
        try {
            std::stringstream ss;
            ss << "Messages: sent=" << get_messages_sent() 
               << ", received=" << get_messages_received()
               << ", errors=" << get_errors()
               << " | Bytes: sent=" << get_bytes_sent()
               << ", received=" << get_bytes_received()
               << " | Avg processing: " << get_average_processing_time_ms() << "ms"
               << " | Rate limits: " << get_rate_limit_exceeded_count();
            return ss.str();
        } catch (...) {
            return "Metrics unavailable";
        }
    }
    
    /**
     * @brief Reset all metrics
     */
    void reset() noexcept {
        messages_sent_.store(0, std::memory_order_relaxed);
        messages_received_.store(0, std::memory_order_relaxed);
        errors_.store(0, std::memory_order_relaxed);
        total_processing_time_.store(0, std::memory_order_relaxed);
        processing_time_count_.store(0, std::memory_order_relaxed);
        max_processing_time_.store(0, std::memory_order_relaxed);
        total_connection_time_.store(0, std::memory_order_relaxed);
        connection_count_.store(0, std::memory_order_relaxed);
        max_connection_time_.store(0, std::memory_order_relaxed);
        rate_limit_exceeded_.store(0, std::memory_order_relaxed);
        bytes_sent_.store(0, std::memory_order_relaxed);
        bytes_received_.store(0, std::memory_order_relaxed);
    }

private:
    // Message counters
    std::atomic<uint64_t> messages_sent_{0};
    std::atomic<uint64_t> messages_received_{0};
    std::atomic<uint64_t> errors_{0};
    
    // Processing time metrics (microseconds)
    std::atomic<uint64_t> total_processing_time_{0};
    std::atomic<uint64_t> processing_time_count_{0};
    std::atomic<uint64_t> max_processing_time_{0};
    
    // Connection duration metrics (seconds)
    std::atomic<uint64_t> total_connection_time_{0};
    std::atomic<uint64_t> connection_count_{0};
    std::atomic<uint64_t> max_connection_time_{0};
    
    // Rate limiting
    std::atomic<uint64_t> rate_limit_exceeded_{0};
    
    // Byte counters
    std::atomic<uint64_t> bytes_sent_{0};
    std::atomic<uint64_t> bytes_received_{0};
};

} // namespace server
} // namespace cppsim