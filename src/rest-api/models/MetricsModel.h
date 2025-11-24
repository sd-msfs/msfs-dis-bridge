/**
 * @file MetricsModel.h
 * @brief Data models for performance metrics and statistics tracking
 */

#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace DISBridge::Models
{

    /**
     * @struct NetworkMetrics
     * @brief Network performance and throughput statistics
     *
     * Tracks UDP multicast transmission statistics including packet counts,
     * byte volumes, error rates, and calculated averages. All counters use
     * atomic types for lock-free concurrent updates from multiple threads.
     *
     * @par Thread Safety
     * Counter fields (atomic) are thread-safe for increments. Average fields
     * are protected by metrics_mutex for read/write operations.
     */
    struct NetworkMetrics
    {
        std::atomic<uint64_t> total_packets_sent{0};       ///< Cumulative PDUs sent via multicast
        std::atomic<uint64_t> total_packets_received{0};   ///< Cumulative PDUs received (if applicable)
        std::atomic<uint64_t> total_bytes_sent{0};         ///< Cumulative bytes transmitted
        std::atomic<uint64_t> total_bytes_received{0};     ///< Cumulative bytes received
        std::atomic<uint64_t> dropped_packets{0};          ///< Packets dropped due to queue overflow
        std::atomic<uint64_t> failed_transmissions{0};     ///< Failed sendto() calls

        mutable std::mutex metrics_mutex;    ///< Protects average calculations
        double avg_latency_ms{0.0};          ///< Moving average network latency in milliseconds
        double packets_per_second{0.0};      ///< Current transmission rate (packets/sec)
        double bytes_per_second{0.0};        ///< Current throughput (bytes/sec)
    };

    /**
     * @struct SimConnectMetrics
     * @brief Microsoft Flight Simulator connection statistics
     *
     * Tracks SimConnect connection reliability, data request patterns,
     * and telemetry streaming rates.
     *
     * @par Thread Safety
     * Counter fields (atomic) are thread-safe. Timing fields are protected
     * by timing_mutex.
     */
    struct SimConnectMetrics
    {
        std::atomic<uint64_t> connection_attempts{0};      ///< Total SimConnect connection attempts
        std::atomic<uint64_t> successful_connections{0};   ///< Successful SimConnect connections
        std::atomic<uint64_t> connection_failures{0};      ///< Failed connection attempts
        std::atomic<uint64_t> data_requests{0};            ///< SimConnect data requests issued
        std::atomic<uint64_t> data_received{0};            ///< SimSample data packets received
        std::atomic<bool> is_connected{false};             ///< Current connection status

        mutable std::mutex timing_mutex;                                 ///< Protects timing calculations
        std::chrono::steady_clock::time_point last_data_received;        ///< Timestamp of last telemetry
        double avg_data_rate_hz{0.0};                                    ///< Telemetry update rate in Hz
    };

    /**
     * @struct ProcessingMetrics
     * @brief DIS PDU encoding/decoding performance statistics
     *
     * Tracks the performance of the encode/decode pipeline including
     * success rates, failure counts, and timing statistics.
     *
     * @par Thread Safety
     * Counter fields (atomic) are thread-safe. Timing averages are protected
     * by timing_mutex.
     */
    struct ProcessingMetrics
    {
        std::atomic<uint64_t> pdus_encoded{0};           ///< Successfully encoded PDUs
        std::atomic<uint64_t> pdus_decoded{0};           ///< Successfully decoded PDUs
        std::atomic<uint64_t> encoding_failures{0};      ///< Encoding operation failures
        std::atomic<uint64_t> decoding_failures{0};      ///< Decoding operation failures

        mutable std::mutex timing_mutex;                 ///< Protects timing calculations
        double avg_encoding_time_ms{0.0};                ///< Moving average encoding time
        double avg_decoding_time_ms{0.0};                ///< Moving average decoding time
        double avg_round_trip_time_ms{0.0};              ///< End-to-end latency from MSFS to network
    };

    /**
     * @struct ResourceMetrics
     * @brief System resource utilization statistics
     *
     * Monitors CPU usage, memory consumption, thread counts, and queue
     * depths to identify performance bottlenecks and resource constraints.
     *
     * @par Thread Safety
     * Resource fields (atomic) are thread-safe. Uptime calculation is
     * protected by uptime_mutex.
     */
    struct ResourceMetrics
    {
        std::atomic<double> cpu_usage_percent{0.0};      ///< Current CPU utilization (0-100%)
        std::atomic<uint64_t> memory_usage_mb{0};        ///< Current memory consumption in MB
        std::atomic<uint32_t> active_threads{0};         ///< Number of active threads
        std::atomic<uint32_t> queue_depth{0};            ///< Current PDU queue depth

        mutable std::mutex uptime_mutex;                               ///< Protects uptime calculation
        std::chrono::steady_clock::time_point start_time;              ///< Application start timestamp
        std::chrono::duration<double> uptime{0};                       ///< Time since application start
    };

    /**
     * @struct ErrorMetrics
     * @brief Error and warning tracking statistics
     *
     * Maintains error counters and a rolling history of recent error messages
     * for diagnostic purposes.
     *
     * @par Thread Safety
     * Counter fields (atomic) are thread-safe. Recent error history is
     * protected by recent_errors_mutex.
     */
    struct ErrorMetrics
    {
        std::atomic<uint64_t> total_errors{0};           ///< Cumulative error count
        std::atomic<uint64_t> total_warnings{0};         ///< Cumulative warning count
        std::atomic<uint64_t> critical_errors{0};        ///< Critical error count

        mutable std::mutex recent_errors_mutex;          ///< Protects error history
        std::vector<std::pair<std::chrono::steady_clock::time_point, std::string>> recent_errors;  ///< Recent error log
        static constexpr size_t MAX_RECENT_ERRORS = 100; ///< Maximum errors retained in history
    };

    /**
     * @class BridgeMetrics
     * @brief Aggregated container for all system metrics
     *
     * Provides a comprehensive metrics collection including network, SimConnect,
     * processing, resource, and error statistics. This class is the primary
     * interface for metrics recording and retrieval.
     *
     * The BridgeMetrics class maintains thread-safe counters and implements
     * moving average calculations with configurable time windows.
     *
     * @par Thread Safety
     * All public methods are thread-safe. Custom copy constructor/assignment
     * handle atomic member copying.
     *
     * @par Example Usage
     * @code
     * BridgeMetrics metrics;
     * metrics.recordPacketSent(144);  // Record 144-byte PDU transmission
     * metrics.recordEncodingTime(0.15);  // 150 microseconds encoding time
     * metrics.updateMovingAverages();  // Recalculate averages
     *
     * std::string json = metrics.toJson();  // Serialize to JSON
     * @endcode
     *
     * @see MetricsCollector
     */
    class BridgeMetrics
    {
    public:
        NetworkMetrics network;          ///< Network transmission metrics
        SimConnectMetrics simconnect;    ///< SimConnect connection metrics
        ProcessingMetrics processing;    ///< PDU encoding/decoding metrics
        ResourceMetrics resources;       ///< System resource utilization
        ErrorMetrics errors;             ///< Error and warning tracking

        /**
         * @brief Default constructor initializes all metrics to zero
         */
        BridgeMetrics();

        /**
         * @brief Copy constructor (custom implementation for atomics)
         *
         * @param other BridgeMetrics instance to copy from
         *
         * @note Required because std::atomic is not copy-constructible by default
         */
        BridgeMetrics(const BridgeMetrics& other);

        /**
         * @brief Copy assignment operator (custom implementation for atomics)
         *
         * @param other BridgeMetrics instance to copy from
         * @return Reference to this instance
         */
        BridgeMetrics& operator=(const BridgeMetrics& other);

        /**
         * @brief Records a packet transmission
         *
         * @param bytes Size of transmitted packet in bytes
         *
         * @par Thread Safety
         * This method is thread-safe and can be called from multiple threads.
         */
        void recordPacketSent(size_t bytes);

        /**
         * @brief Records a packet reception
         *
         * @param bytes Size of received packet in bytes
         */
        void recordPacketReceived(size_t bytes);

        /**
         * @brief Records network latency measurement
         *
         * @param latency_ms Latency in milliseconds
         *
         * @note Latency is added to moving average history
         */
        void recordLatency(double latency_ms);

        /**
         * @brief Records PDU encoding operation time
         *
         * @param time_ms Encoding duration in milliseconds
         *
         * @note Times are used for moving average calculation
         */
        void recordEncodingTime(double time_ms);

        /**
         * @brief Records PDU decoding operation time
         *
         * @param time_ms Decoding duration in milliseconds
         */
        void recordDecodingTime(double time_ms);

        /**
         * @brief Records an error event
         *
         * @param error_message Descriptive error message
         *
         * @note Errors are added to recent error history (max 100 entries)
         */
        void recordError(const std::string &error_message);

        /**
         * @brief Records a warning event
         *
         * @param warning_message Descriptive warning message
         */
        void recordWarning(const std::string &warning_message);

        /**
         * @brief Recalculates all moving averages from history
         *
         * Should be called periodically (e.g., once per second) to update
         * average metrics like packets_per_second, avg_latency_ms, etc.
         *
         * @note This operation locks history_mutex
         */
        void updateMovingAverages();

        /**
         * @brief Calculates current application uptime
         *
         * @return Duration since application start
         */
        std::chrono::duration<double> getUptime() const;

        /**
         * @brief Serializes all metrics to JSON string
         *
         * @return JSON representation of all metrics
         *
         * @par Example Output
         * @code
         * {
         *   "network": {"packets_sent": 12345, "bytes_sent": 1234567, ...},
         *   "simconnect": {"is_connected": true, ...},
         *   "processing": {"pdus_encoded": 12345, ...},
         *   "resources": {"cpu_percent": 25.5, "memory_mb": 128, ...},
         *   "errors": {"total_errors": 0, ...}
         * }
         * @endcode
         */
        std::string toJson() const;

    private:
        /**
         * @brief Updates network-specific moving averages
         */
        void updateNetworkAverages();

        /**
         * @brief Updates processing performance moving averages
         */
        void updateProcessingAverages();

        /**
         * @brief Removes error entries older than retention window
         */
        void cleanupOldErrors();

        mutable std::mutex history_mutex;    ///< Protects historical data structures
        std::vector<std::pair<std::chrono::steady_clock::time_point, double>> latency_history;  ///< Latency sample history
        std::vector<std::pair<std::chrono::steady_clock::time_point, size_t>> throughput_history;  ///< Throughput sample history
        static constexpr size_t MAX_HISTORY_SIZE = 1000;              ///< Maximum history entries
        static constexpr std::chrono::seconds HISTORY_WINDOW{300};    ///< 5-minute rolling window
    };

}