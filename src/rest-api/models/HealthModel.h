/**
 * @file HealthModel.h
 * @brief Data models for system health monitoring and status tracking
 */

#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>

namespace DISBridge::Models
{

    /**
     * @enum HealthStatus
     * @brief Overall health status levels for components and system
     *
     * These status levels indicate the operational state of system components:
     * - HEALTHY: Component operating normally within all thresholds
     * - WARNING: Component experiencing degraded performance or approaching limits
     * - CRITICAL: Component failure or severe issues requiring immediate attention
     * - UNKNOWN: Unable to determine health status (initialization or connection lost)
     */
    enum class HealthStatus
    {
        HEALTHY,    ///< All systems operating normally
        WARNING,    ///< Degraded performance or approaching thresholds
        CRITICAL,   ///< Component failure or severe issues
        UNKNOWN     ///< Status cannot be determined
    };

    /**
     * @enum ComponentType
     * @brief System components monitored for health status
     *
     * Each component represents a major subsystem that can be independently
     * monitored for health and performance.
     */
    enum class ComponentType
    {
        SIMCONNECT,       ///< SimConnect connection to Microsoft Flight Simulator
        DIS_NETWORK,      ///< DIS PDU network transmission and reception
        REST_API,         ///< HTTP REST API server responsiveness
        BRIDGE_CORE,      ///< Core encoding/decoding and mapping logic
        SYSTEM_RESOURCES  ///< CPU, memory, and system resource availability
    };

    /**
     * @struct ComponentHealth
     * @brief Health status information for a single system component
     *
     * Tracks the current health status of an individual component along with
     * diagnostic information and timing data.
     */
    struct ComponentHealth
    {
        ComponentType component;                                ///< Component identifier
        HealthStatus status;                                    ///< Current health status
        std::string message;                                    ///< Human-readable status message
        std::chrono::steady_clock::time_point last_check;       ///< Timestamp of last health check
        std::unordered_map<std::string, std::string> details;   ///< Additional diagnostic details

        /**
         * @brief Constructs component health with default status
         *
         * @param comp Component type identifier
         * @param stat Initial health status (default: UNKNOWN)
         */
        ComponentHealth(ComponentType comp, HealthStatus stat = HealthStatus::UNKNOWN)
            : component(comp), status(stat), last_check(std::chrono::steady_clock::now()) {}
    };

    /**
     * @struct SystemHealth
     * @brief Aggregated health status for the entire bridge system
     *
     * Provides a comprehensive health snapshot including overall status,
     * individual component health, version information, and uptime.
     *
     * The overall status is determined by the worst (most critical) component
     * status:
     * - If any component is CRITICAL, overall is CRITICAL
     * - If any component is WARNING (and none CRITICAL), overall is WARNING
     * - If all components are HEALTHY, overall is HEALTHY
     * - Otherwise, overall is UNKNOWN
     */
    struct SystemHealth
    {
        HealthStatus overall_status;                         ///< Aggregated health across all components
        std::chrono::steady_clock::time_point timestamp;     ///< Timestamp of health snapshot
        std::vector<ComponentHealth> components;             ///< Health status of each component
        std::string version;                                 ///< Application version string
        std::chrono::duration<double> uptime;                ///< Time since application start

        /**
         * @brief Default constructor initializes with UNKNOWN status
         */
        SystemHealth() : overall_status(HealthStatus::UNKNOWN),
                         timestamp(std::chrono::steady_clock::now()),
                         version("1.0.0") {}

        /**
         * @brief Calculates overall system status from component statuses
         *
         * Determines the aggregate health by examining all component statuses
         * and selecting the most critical level.
         *
         * @note Should be called after updating component health values
         */
        void calculateOverallStatus();

        /**
         * @brief Serializes health data to JSON string
         *
         * @return JSON representation of system health
         *
         * @par Example Output
         * @code
         * {
         *   "status": "HEALTHY",
         *   "version": "1.0.0",
         *   "uptime": 3600.5,
         *   "components": [
         *     {"component": "SIMCONNECT", "status": "HEALTHY", "message": ""},
         *     ...
         *   ]
         * }
         * @endcode
         */
        std::string toJson() const;

        /**
         * @brief Updates or adds component health information
         *
         * Finds the specified component in the components vector and updates
         * its status, message, and details. If the component doesn't exist,
         * it is added.
         *
         * @param component Component type to update
         * @param status New health status
         * @param message Human-readable status description
         * @param details Additional diagnostic key-value pairs
         */
        void updateComponent(ComponentType component, HealthStatus status,
                             const std::string &message = "",
                             const std::unordered_map<std::string, std::string> &details = {});
    };

    /**
     * @struct HealthThresholds
     * @brief Configurable thresholds for health status determination
     *
     * Defines the limits and thresholds used to determine when components
     * transition between HEALTHY, WARNING, and CRITICAL states.
     *
     * These thresholds are checked against current metrics to automatically
     * determine component health status.
     */
    struct HealthThresholds
    {
        double max_cpu_percent = 80.0;                     ///< CPU usage warning threshold (%)
        uint64_t max_memory_mb = 512;                      ///< Memory usage warning threshold (MB)
        uint32_t max_queue_depth = 1000;                   ///< PDU queue depth warning threshold
        double max_avg_latency_ms = 100.0;                 ///< Network latency warning threshold (ms)
        uint64_t max_consecutive_failures = 5;             ///< Consecutive failure CRITICAL threshold
        std::chrono::seconds component_timeout{30};        ///< Component timeout for UNKNOWN status

        /**
         * @brief Loads threshold values from configuration file
         *
         * Reads threshold settings from a configuration file if provided,
         * otherwise uses default values.
         *
         * @param config_path Path to configuration file (empty = use defaults)
         */
        void loadFromConfig(const std::string &config_path = "");
    };

    /**
     * @brief Converts HealthStatus enum to string representation
     *
     * @param status Health status value
     * @return String name ("HEALTHY", "WARNING", "CRITICAL", "UNKNOWN")
     */
    std::string healthStatusToString(HealthStatus status);

    /**
     * @brief Converts ComponentType enum to string representation
     *
     * @param type Component type value
     * @return String name ("SIMCONNECT", "DIS_NETWORK", "REST_API", etc.)
     */
    std::string componentTypeToString(ComponentType type);
    HealthStatus stringToHealthStatus(const std::string &status_str);

}