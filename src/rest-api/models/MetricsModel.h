#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace DISBridge::Models
{

    // Network performance metrics
    struct NetworkMetrics
    {
        std::atomic<uint64_t> total_packets_sent{0};
        std::atomic<uint64_t> total_packets_received{0};
        std::atomic<uint64_t> total_bytes_sent{0};
        std::atomic<uint64_t> total_bytes_received{0};
        std::atomic<uint64_t> dropped_packets{0};
        std::atomic<uint64_t> failed_transmissions{0};

        // Moving averages (protected by mutex)
        mutable std::mutex metrics_mutex;
        double avg_latency_ms{0.0};
        double packets_per_second{0.0};
        double bytes_per_second{0.0};
    };

    // SimConnect specific metrics
    struct SimConnectMetrics
    {
        std::atomic<uint64_t> connection_attempts{0};
        std::atomic<uint64_t> successful_connections{0};
        std::atomic<uint64_t> connection_failures{0};
        std::atomic<uint64_t> data_requests{0};
        std::atomic<uint64_t> data_received{0};
        std::atomic<bool> is_connected{false};

        mutable std::mutex timing_mutex;
        std::chrono::steady_clock::time_point last_data_received;
        double avg_data_rate_hz{0.0};
    };

    // DIS Bridge processing metrics
    struct ProcessingMetrics
    {
        std::atomic<uint64_t> pdus_encoded{0};
        std::atomic<uint64_t> pdus_decoded{0};
        std::atomic<uint64_t> encoding_failures{0};
        std::atomic<uint64_t> decoding_failures{0};

        mutable std::mutex timing_mutex;
        double avg_encoding_time_ms{0.0};
        double avg_decoding_time_ms{0.0};
        double avg_round_trip_time_ms{0.0};
    };

    // System resource metrics
    struct ResourceMetrics
    {
        std::atomic<double> cpu_usage_percent{0.0};
        std::atomic<uint64_t> memory_usage_mb{0};
        std::atomic<uint32_t> active_threads{0};
        std::atomic<uint32_t> queue_depth{0};

        mutable std::mutex uptime_mutex;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::duration<double> uptime{0};
    };

    // Error and warning tracking
    struct ErrorMetrics
    {
        std::atomic<uint64_t> total_errors{0};
        std::atomic<uint64_t> total_warnings{0};
        std::atomic<uint64_t> critical_errors{0};

        mutable std::mutex recent_errors_mutex;
        std::vector<std::pair<std::chrono::steady_clock::time_point, std::string>> recent_errors;
        static constexpr size_t MAX_RECENT_ERRORS = 100;
    };

    // Aggregated metrics container
    class BridgeMetrics
    {
    public:
        NetworkMetrics network;
        SimConnectMetrics simconnect;
        ProcessingMetrics processing;
        ResourceMetrics resources;
        ErrorMetrics errors;

        BridgeMetrics();

        // Copy constructor and assignment operator (custom because of atomics)
        BridgeMetrics(const BridgeMetrics& other);
        BridgeMetrics& operator=(const BridgeMetrics& other);

        // Update methods
        void recordPacketSent(size_t bytes);
        void recordPacketReceived(size_t bytes);
        void recordLatency(double latency_ms);
        void recordEncodingTime(double time_ms);
        void recordDecodingTime(double time_ms);
        void recordError(const std::string &error_message);
        void recordWarning(const std::string &warning_message);

        // Calculation methods
        void updateMovingAverages();
        std::chrono::duration<double> getUptime() const;

        // JSON serialization
        std::string toJson() const;

    private:
        void updateNetworkAverages();
        void updateProcessingAverages();
        void cleanupOldErrors();

        // Moving average history
        mutable std::mutex history_mutex;
        std::vector<std::pair<std::chrono::steady_clock::time_point, double>> latency_history;
        std::vector<std::pair<std::chrono::steady_clock::time_point, size_t>> throughput_history;
        static constexpr size_t MAX_HISTORY_SIZE = 1000;
        static constexpr std::chrono::seconds HISTORY_WINDOW{300}; // 5 minutes
    };

}