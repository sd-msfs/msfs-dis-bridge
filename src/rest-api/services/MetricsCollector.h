#pragma once

#include "MetricsModel.h"
#include "HealthModel.h"
#include <memory>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <functional>

namespace DISBridge::Services {

    class MetricsCollector {
    public:
        MetricsCollector();
        ~MetricsCollector();

        // Singleton access
        static MetricsCollector& getInstance();

        // Lifecycle management
        void start();
        void stop();
        bool isRunning() const { return running_.load(); }

        // Metrics access
        const Models::BridgeMetrics& getMetrics() const { return metrics_; }
        Models::BridgeMetrics getMetricsSnapshot() const;

        // Health monitoring
        const Models::SystemHealth& getHealth() const { return health_; }
        Models::SystemHealth getHealthSnapshot() const;

        // Event recording methods
        void recordPacketSent(size_t bytes);
        void recordPacketReceived(size_t bytes);
        void recordDroppedPacket();
        void recordLatency(double latency_ms);
        void recordEncodingTime(double time_ms);
        void recordDecodingTime(double time_ms);
        void recordSimConnectEvent(const std::string& event_type);
        void recordError(const std::string& component, const std::string& error_message);
        void recordWarning(const std::string& component, const std::string& warning_message);

        // Component health updates
        void updateComponentHealth(Models::ComponentType component, 
                                 Models::HealthStatus status,
                                 const std::string& message = "",
                                 const std::unordered_map<std::string, std::string>& details = {});

        // Configuration
        void setUpdateInterval(std::chrono::milliseconds interval) { update_interval_ = interval; }
        void setHealthThresholds(const Models::HealthThresholds& thresholds) { thresholds_ = thresholds; }

        // Callbacks for external integrations
        using MetricsCallback = std::function<void(const Models::BridgeMetrics&)>;
        using HealthCallback = std::function<void(const Models::SystemHealth&)>;
        
        void registerMetricsCallback(const MetricsCallback& callback);
        void registerHealthCallback(const HealthCallback& callback);

    private:
        // Core metrics and health data
        mutable std::mutex metrics_mutex_;
        Models::BridgeMetrics metrics_;
        Models::SystemHealth health_;
        Models::HealthThresholds thresholds_;

        // Background monitoring thread
        std::atomic<bool> running_{false};
        std::thread monitoring_thread_;
        std::condition_variable cv_;
        std::mutex cv_mutex_;
        std::chrono::milliseconds update_interval_{1000}; // 1 second default

        // Callbacks
        std::mutex callbacks_mutex_;
        std::vector<MetricsCallback> metrics_callbacks_;
        std::vector<HealthCallback> health_callbacks_;

        // Internal methods
        void monitoringLoop();
        void updateMetrics();
        void updateHealth();
        void checkComponentHealth();
        void notifyCallbacks();

        // System resource monitoring
        void updateSystemResources();
        double getCurrentCpuUsage();
        uint64_t getCurrentMemoryUsage();

        // Singleton implementation
        static std::unique_ptr<MetricsCollector> instance_;
        static std::mutex instance_mutex_;
    };

    // RAII helper for timing operations
    class ScopedTimer {
    public:
        using TimeCallback = std::function<void(double)>;
        
        explicit ScopedTimer(TimeCallback callback)
            : callback_(std::move(callback))
            , start_time_(std::chrono::high_resolution_clock::now()) {}
        
        ~ScopedTimer() {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double, std::milli>(end_time - start_time_);
            callback_(duration.count());
        }

    private:
        TimeCallback callback_;
        std::chrono::high_resolution_clock::time_point start_time_;
    };

    // Convenience macros for timing
    #define TIME_ENCODING(collector) \
        ScopedTimer timer([&collector](double time_ms) { \
            collector.recordEncodingTime(time_ms); \
        })

    #define TIME_DECODING(collector) \
        ScopedTimer timer([&collector](double time_ms) { \
            collector.recordDecodingTime(time_ms); \
        })

}