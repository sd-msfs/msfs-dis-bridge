#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>

namespace DISBridge::Models
{

    enum class HealthStatus
    {
        HEALTHY,
        WARNING,
        CRITICAL,
        UNKNOWN
    };

    enum class ComponentType
    {
        SIMCONNECT,
        DIS_NETWORK,
        REST_API,
        BRIDGE_CORE,
        SYSTEM_RESOURCES
    };

    struct ComponentHealth
    {
        ComponentType component;
        HealthStatus status;
        std::string message;
        std::chrono::steady_clock::time_point last_check;
        std::unordered_map<std::string, std::string> details;

        ComponentHealth(ComponentType comp, HealthStatus stat = HealthStatus::UNKNOWN)
            : component(comp), status(stat), last_check(std::chrono::steady_clock::now()) {}
    };

    struct SystemHealth
    {
        HealthStatus overall_status;
        std::chrono::steady_clock::time_point timestamp;
        std::vector<ComponentHealth> components;
        std::string version;
        std::chrono::duration<double> uptime;

        SystemHealth() : overall_status(HealthStatus::UNKNOWN),
                         timestamp(std::chrono::steady_clock::now()),
                         version("1.0.0") {}

        // Calculate overall status based on component statuses
        void calculateOverallStatus();

        // JSON serialization
        std::string toJson() const;

        // Add or update component health
        void updateComponent(ComponentType component, HealthStatus status,
                             const std::string &message = "",
                             const std::unordered_map<std::string, std::string> &details = {});
    };

    // Health check thresholds and configuration
    struct HealthThresholds
    {
        double max_cpu_percent = 80.0;
        uint64_t max_memory_mb = 512;
        uint32_t max_queue_depth = 1000;
        double max_avg_latency_ms = 100.0;
        uint64_t max_consecutive_failures = 5;
        std::chrono::seconds component_timeout{30};

        // Load from configuration
        void loadFromConfig(const std::string &config_path = "");
    };

    // Utility functions
    std::string healthStatusToString(HealthStatus status);
    std::string componentTypeToString(ComponentType type);
    HealthStatus stringToHealthStatus(const std::string &status_str);

}