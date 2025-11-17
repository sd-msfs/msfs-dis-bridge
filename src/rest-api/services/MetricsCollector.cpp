#include "MetricsCollector.h"
#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <mutex>

#pragma comment(lib, "psapi.lib")

namespace DISBridge::Services
{

    std::unique_ptr<MetricsCollector> MetricsCollector::instance_ = nullptr;
    std::mutex MetricsCollector::instance_mutex_;

    MetricsCollector& MetricsCollector::getInstance()
    {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        if (!instance_)
        {
            instance_.reset(new MetricsCollector());
        }
        return *instance_;
    }

    MetricsCollector::MetricsCollector()
    {
        // Initialize health with all components
        health_.updateComponent(Models::ComponentType::SIMCONNECT,
                                Models::HealthStatus::UNKNOWN,
                                "Not initialized");

        health_.updateComponent(Models::ComponentType::DIS_NETWORK,
                                Models::HealthStatus::UNKNOWN,
                                "Not initialized");

        health_.updateComponent(Models::ComponentType::REST_API,
                                Models::HealthStatus::HEALTHY,
                                "REST API initialized");

        health_.updateComponent(Models::ComponentType::BRIDGE_CORE,
                                Models::HealthStatus::UNKNOWN,
                                "Not initialized");

        health_.updateComponent(Models::ComponentType::SYSTEM_RESOURCES,
                                Models::HealthStatus::HEALTHY,
                                "Monitoring started");
    }

    MetricsCollector::~MetricsCollector()
    {
        stop();
    }

    // ========================================================================
    // Lifecycle Management
    // ========================================================================

    void MetricsCollector::start()
    {
        if (running_.load())
        {
            return; // Already running
        }

        running_.store(true);
        monitoring_thread_ = std::thread(&MetricsCollector::monitoringLoop, this);

        std::cout << "[MetricsCollector] Started monitoring service" << std::endl;
    }

    void MetricsCollector::stop()
    {
        if (!running_.load())
        {
            return; // Not running
        }

        running_.store(false);
        cv_.notify_all();

        if (monitoring_thread_.joinable())
        {
            monitoring_thread_.join();
        }

        std::cout << "[MetricsCollector] Stopped monitoring service" << std::endl;
    }

    // ========================================================================
    // Metrics Access
    // ========================================================================

    Models::BridgeMetrics MetricsCollector::getMetricsSnapshot() const
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        return metrics_;
    }

    Models::SystemHealth MetricsCollector::getHealthSnapshot() const
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        return health_;
    }

    // ========================================================================
    // Event Recording Methods
    // ========================================================================

    void MetricsCollector::recordPacketSent(size_t bytes)
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.recordPacketSent(bytes);
    }

    void MetricsCollector::recordPacketReceived(size_t bytes)
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.recordPacketReceived(bytes);
    }

    void MetricsCollector::recordDroppedPacket()
    {
        metrics_.network.dropped_packets.fetch_add(1, std::memory_order_relaxed);

        // Check if we're dropping too many packets
        auto total_packets = metrics_.network.total_packets_received.load() +
                             metrics_.network.total_packets_sent.load();
        auto dropped = metrics_.network.dropped_packets.load();

        if (total_packets > 100 && (dropped * 100.0 / total_packets) > 5.0)
        {
            updateComponentHealth(Models::ComponentType::DIS_NETWORK,
                                  Models::HealthStatus::WARNING,
                                  "High packet drop rate detected");
        }
    }

    void MetricsCollector::recordLatency(double latency_ms)
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.recordLatency(latency_ms);

        // Check against thresholds
        if (latency_ms > thresholds_.max_avg_latency_ms)
        {
            updateComponentHealth_Locked(Models::ComponentType::DIS_NETWORK,
                                        Models::HealthStatus::WARNING,
                                        "High network latency detected");
        }
    }

    void MetricsCollector::recordEncodingTime(double time_ms)
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.recordEncodingTime(time_ms);
    }

    void MetricsCollector::recordDecodingTime(double time_ms)
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.recordDecodingTime(time_ms);
    }

    void MetricsCollector::recordSimConnectEvent(const std::string& event_type)
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);

        if (event_type == "connection_attempt")
        {
            metrics_.simconnect.connection_attempts.fetch_add(1, std::memory_order_relaxed);
        }
        else if (event_type == "connection_success")
        {
            metrics_.simconnect.successful_connections.fetch_add(1, std::memory_order_relaxed);
            metrics_.simconnect.is_connected.store(true, std::memory_order_relaxed);

            updateComponentHealth_Locked(Models::ComponentType::SIMCONNECT,
                                        Models::HealthStatus::HEALTHY,
                                        "Connected to SimConnect");
        }
        else if (event_type == "connection_failure")
        {
            metrics_.simconnect.connection_failures.fetch_add(1, std::memory_order_relaxed);
            metrics_.simconnect.is_connected.store(false, std::memory_order_relaxed);

            updateComponentHealth_Locked(Models::ComponentType::SIMCONNECT,
                                        Models::HealthStatus::CRITICAL,
                                        "Failed to connect to SimConnect");
        }
        else if (event_type == "data_request")
        {
            metrics_.simconnect.data_requests.fetch_add(1, std::memory_order_relaxed);
        }
        else if (event_type == "data_received")
        {
            metrics_.simconnect.data_received.fetch_add(1, std::memory_order_relaxed);
            std::lock_guard<std::mutex> timing_lock(metrics_.simconnect.timing_mutex);
            metrics_.simconnect.last_data_received = std::chrono::steady_clock::now();
        }
    }

    void MetricsCollector::recordError(const std::string& component, const std::string& error_message)
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.recordError("[" + component + "] " + error_message);

        // Update component health based on error
        Models::ComponentType comp_type = Models::ComponentType::BRIDGE_CORE;

        if (component == "simconnect")
            comp_type = Models::ComponentType::SIMCONNECT;
        else if (component == "network" || component == "dis")
            comp_type = Models::ComponentType::DIS_NETWORK;
        else if (component == "api" || component == "rest")
            comp_type = Models::ComponentType::REST_API;

        updateComponentHealth_Locked(comp_type, Models::HealthStatus::CRITICAL, error_message);
    }

    void MetricsCollector::recordWarning(const std::string& component, const std::string& warning_message)
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.recordWarning("[" + component + "] " + warning_message);
    }

    // ========================================================================
    // Metrics Management
    // ========================================================================

    void MetricsCollector::resetMetrics()
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);

        // Create new metrics instance (resets everything)
        metrics_ = Models::BridgeMetrics();

        std::cout << "[MetricsCollector] All metrics have been reset" << std::endl;
    }

    void MetricsCollector::resetCounters()
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);

        // Reset network counters
        metrics_.network.total_packets_sent.store(0);
        metrics_.network.total_packets_received.store(0);
        metrics_.network.total_bytes_sent.store(0);
        metrics_.network.total_bytes_received.store(0);
        metrics_.network.dropped_packets.store(0);
        metrics_.network.failed_transmissions.store(0);

        // Reset SimConnect counters
        metrics_.simconnect.connection_attempts.store(0);
        metrics_.simconnect.successful_connections.store(0);
        metrics_.simconnect.connection_failures.store(0);
        metrics_.simconnect.data_requests.store(0);
        metrics_.simconnect.data_received.store(0);

        // Reset processing counters
        metrics_.processing.pdus_encoded.store(0);
        metrics_.processing.pdus_decoded.store(0);
        metrics_.processing.encoding_failures.store(0);
        metrics_.processing.decoding_failures.store(0);

        // Reset error counters
        metrics_.errors.total_errors.store(0);
        metrics_.errors.total_warnings.store(0);
        metrics_.errors.critical_errors.store(0);

        std::cout << "[MetricsCollector] Counters have been reset (uptime and averages preserved)" << std::endl;
    }

    // ========================================================================
    // Component Health Updates
    // ========================================================================

    void MetricsCollector::updateComponentHealth(Models::ComponentType component,
                                                 Models::HealthStatus status,
                                                 const std::string& message,
                                                 const std::unordered_map<std::string, std::string>& details)
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        health_.updateComponent(component, status, message, details);
    }

    // Internal helper - assumes metrics_mutex_ is already locked by caller
    void MetricsCollector::updateComponentHealth_Locked(Models::ComponentType component,
                                                        Models::HealthStatus status,
                                                        const std::string& message,
                                                        const std::unordered_map<std::string, std::string>& details)
    {
        health_.updateComponent(component, status, message, details);
    }

    // ========================================================================
    // Callback Registration
    // ========================================================================

    void MetricsCollector::registerMetricsCallback(const MetricsCallback& callback)
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        metrics_callbacks_.push_back(callback);
    }

    void MetricsCollector::registerHealthCallback(const HealthCallback& callback)
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        health_callbacks_.push_back(callback);
    }

    // ========================================================================
    // Background Monitoring Loop
    // ========================================================================

    void MetricsCollector::monitoringLoop()
    {
        while (running_.load())
        {
            // Wait for the update interval or stop signal
            {
                std::unique_lock<std::mutex> lock(cv_mutex_);
                cv_.wait_for(lock, update_interval_, [this]() { return !running_.load(); });
            }

            if (!running_.load())
            {
                break;
            }

            // Update metrics and health
            updateMetrics();
            updateHealth();
            notifyCallbacks();
        }
    }

    void MetricsCollector::updateMetrics()
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);

        // Update moving averages
        metrics_.updateMovingAverages();

        // Update system resources
        updateSystemResources();
    }

    void MetricsCollector::updateHealth()
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);

        // Update uptime
        health_.uptime = metrics_.getUptime();

        // Check component health based on metrics
        checkComponentHealth();

        // Recalculate overall health status
        health_.calculateOverallStatus();
    }

    void MetricsCollector::checkComponentHealth()
    {
        // Check system resources
        double cpu_usage = metrics_.resources.cpu_usage_percent.load();
        uint64_t memory_mb = metrics_.resources.memory_usage_mb.load();

        if (cpu_usage > thresholds_.max_cpu_percent || memory_mb > thresholds_.max_memory_mb)
        {
            std::string message = "Resource usage high: CPU=" + std::to_string(cpu_usage) +
                                  "%, Memory=" + std::to_string(memory_mb) + "MB";

            health_.updateComponent(Models::ComponentType::SYSTEM_RESOURCES,
                                    Models::HealthStatus::WARNING,
                                    message);
        }
        else
        {
            health_.updateComponent(Models::ComponentType::SYSTEM_RESOURCES,
                                    Models::HealthStatus::HEALTHY,
                                    "Resource usage normal");
        }

        // Check SimConnect connectivity
        bool is_connected = metrics_.simconnect.is_connected.load();
        if (!is_connected)
        {
            uint64_t failures = metrics_.simconnect.connection_failures.load();
            if (failures > thresholds_.max_consecutive_failures)
            {
                health_.updateComponent(Models::ComponentType::SIMCONNECT,
                                        Models::HealthStatus::CRITICAL,
                                        "Multiple connection failures");
            }
        }

        // Check DIS network health (checks for packet activity and errors)
        uint64_t dropped = metrics_.network.dropped_packets.load();
        uint64_t total = metrics_.network.total_packets_received.load() +
                         metrics_.network.total_packets_sent.load();
        uint64_t pdus_encoded = metrics_.processing.pdus_encoded.load();
        uint64_t encoding_failures = metrics_.processing.encoding_failures.load();

        if (total > 0)
        {
            double drop_rate = (dropped * 100.0) / total;
            if (drop_rate > 10.0)
            {
                health_.updateComponent(Models::ComponentType::DIS_NETWORK,
                                        Models::HealthStatus::CRITICAL,
                                        "Critical packet drop rate: " + std::to_string(drop_rate) + "%");
            }
            else if (drop_rate > 5.0)
            {
                health_.updateComponent(Models::ComponentType::DIS_NETWORK,
                                        Models::HealthStatus::WARNING,
                                        "Elevated packet drop rate: " + std::to_string(drop_rate) + "%");
            }
            else
            {
                health_.updateComponent(Models::ComponentType::DIS_NETWORK,
                                        Models::HealthStatus::HEALTHY,
                                        "Network operating normally (" + std::to_string(total) + " packets processed)");
            }
        }
        else
        {
            // No network activity yet
            health_.updateComponent(Models::ComponentType::DIS_NETWORK,
                                    Models::HealthStatus::HEALTHY,
                                    "No network activity yet");
        }

        // Check bridge core health (encoding/decoding operations)
        if (pdus_encoded > 0 || encoding_failures > 0)
        {
            uint64_t decoding_failures = metrics_.processing.decoding_failures.load();
            uint64_t total_ops = pdus_encoded + encoding_failures;

            if (total_ops > 0)
            {
                double failure_rate = ((encoding_failures + decoding_failures) * 100.0) / total_ops;
                if (failure_rate > 10.0)
                {
                    health_.updateComponent(Models::ComponentType::BRIDGE_CORE,
                                            Models::HealthStatus::CRITICAL,
                                            "High PDU processing failure rate: " + std::to_string(failure_rate) + "%");
                }
                else if (failure_rate > 5.0)
                {
                    health_.updateComponent(Models::ComponentType::BRIDGE_CORE,
                                            Models::HealthStatus::WARNING,
                                            "Elevated PDU processing failures");
                }
                else
                {
                    health_.updateComponent(Models::ComponentType::BRIDGE_CORE,
                                            Models::HealthStatus::HEALTHY,
                                            "Bridge processing normally (" + std::to_string(pdus_encoded) + " PDUs encoded)");
                }
            }
        }
        else
        {
            // No processing activity yet
            health_.updateComponent(Models::ComponentType::BRIDGE_CORE,
                                    Models::HealthStatus::HEALTHY,
                                    "No PDU processing activity yet");
        }
    }

    void MetricsCollector::notifyCallbacks()
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);

        // Notify metrics callbacks
        if (!metrics_callbacks_.empty())
        {
            Models::BridgeMetrics snapshot = getMetricsSnapshot();
            for (const auto& callback : metrics_callbacks_)
            {
                try
                {
                    callback(snapshot);
                }
                catch (const std::exception& e)
                {
                    std::cerr << "[MetricsCollector] Callback error: " << e.what() << std::endl;
                }
            }
        }

        // Notify health callbacks
        if (!health_callbacks_.empty())
        {
            Models::SystemHealth health_snapshot = getHealthSnapshot();
            for (const auto& callback : health_callbacks_)
            {
                try
                {
                    callback(health_snapshot);
                }
                catch (const std::exception& e)
                {
                    std::cerr << "[MetricsCollector] Health callback error: " << e.what() << std::endl;
                }
            }
        }
    }

    // ========================================================================
    // System Resource Monitoring (Windows-specific)
    // ========================================================================

    void MetricsCollector::updateSystemResources()
    {
        // Update CPU usage
        double cpu_usage = getCurrentCpuUsage();
        metrics_.resources.cpu_usage_percent.store(cpu_usage, std::memory_order_relaxed);

        // Update memory usage
        uint64_t memory_mb = getCurrentMemoryUsage();
        metrics_.resources.memory_usage_mb.store(memory_mb, std::memory_order_relaxed);

        // Update thread count (approximate)
        HANDLE hProcess = GetCurrentProcess();
        DWORD threadCount = 0;

        // Get thread count via process information
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);

        // Note: This is a simplified approach. For accurate thread count,
        // you'd need to enumerate threads using Tool Help API
        metrics_.resources.active_threads.store(static_cast<uint32_t>(threadCount),
                                                std::memory_order_relaxed);
    }

    double MetricsCollector::getCurrentCpuUsage()
    {
        // Simplified CPU usage calculation for Windows
        // possibly use PDH (Performance Data Helper) API in future prod
        static ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
        static int numProcessors = 0;
        static HANDLE self = GetCurrentProcess();
        static bool initialized = false;

        if (!initialized)
        {
            SYSTEM_INFO sysInfo;
            GetSystemInfo(&sysInfo);
            numProcessors = sysInfo.dwNumberOfProcessors;

            FILETIME ftime, fsys, fuser;
            GetSystemTimeAsFileTime(&ftime);

            std::copy(reinterpret_cast<const char*>(&ftime),
                     reinterpret_cast<const char*>(&ftime) + sizeof(FILETIME),
                     reinterpret_cast<char*>(&lastCPU));

            GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
            std::copy(reinterpret_cast<const char*>(&fsys),
                     reinterpret_cast<const char*>(&fsys) + sizeof(FILETIME),
                     reinterpret_cast<char*>(&lastSysCPU));
            std::copy(reinterpret_cast<const char*>(&fuser),
                     reinterpret_cast<const char*>(&fuser) + sizeof(FILETIME),
                     reinterpret_cast<char*>(&lastUserCPU));

            initialized = true;
            return 0.0;
        }

        FILETIME ftime, fsys, fuser;
        ULARGE_INTEGER now, sys, user;

        GetSystemTimeAsFileTime(&ftime);
        std::copy(reinterpret_cast<const char*>(&ftime),
                 reinterpret_cast<const char*>(&ftime) + sizeof(FILETIME),
                 reinterpret_cast<char*>(&now));

        GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
        std::copy(reinterpret_cast<const char*>(&fsys),
                 reinterpret_cast<const char*>(&fsys) + sizeof(FILETIME),
                 reinterpret_cast<char*>(&sys));
        std::copy(reinterpret_cast<const char*>(&fuser),
                 reinterpret_cast<const char*>(&fuser) + sizeof(FILETIME),
                 reinterpret_cast<char*>(&user));

        double percent = 0.0;
        if (now.QuadPart != lastCPU.QuadPart)
        {
            percent = static_cast<double>((sys.QuadPart - lastSysCPU.QuadPart) +
                                          (user.QuadPart - lastUserCPU.QuadPart));
            percent /= (now.QuadPart - lastCPU.QuadPart);
            percent /= numProcessors;
            percent *= 100.0;
        }

        lastCPU = now;
        lastUserCPU = user;
        lastSysCPU = sys;

        return percent;
    }

    uint64_t MetricsCollector::getCurrentMemoryUsage()
    {
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(),
                                 reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                                 sizeof(pmc)))
        {
            // Return memory in MB
            return static_cast<uint64_t>(pmc.WorkingSetSize / (1024 * 1024));
        }

        return 0;
    }

}
