/**
 * @file MetricsController.h
 * @brief REST API controllers for system metrics and health monitoring
 */

#pragma once

#include <crow.h>
#include <string>
#include <chrono>
#include <mutex>

// Forward declarations to avoid circular includes
namespace DISBridge
{
    namespace Models
    {
        struct NetworkMetrics;
        struct ProcessingMetrics;
        struct ResourceMetrics;
        struct ErrorMetrics;
        struct BridgeMetrics;
        struct SystemHealth;
        struct ComponentHealth;
        enum class HealthStatus;
        enum class ComponentType;
    }

    namespace Services
    {
        class MetricsCollector;
    }
}

namespace DISBridge::Controllers
{

    /**
     * @class MetricsController
     * @brief HTTP REST controller for performance and resource metrics
     *
     * This controller exposes endpoints for querying system performance metrics,
     * including network statistics, processing performance, resource usage, and
     * error tracking. It integrates with the MetricsCollector service to provide
     * real-time and historical metrics data.
     *
     * Metrics categories:
     * - Network: Packet counts, throughput, latency, dropped packets
     * - Processing: Encoding/decoding times, PDU processing rates
     * - Resources: CPU usage, memory consumption, thread counts, queue depth
     * - Errors: Error/warning counts, recent error history
     *
     * @par HTTP Endpoints
     * - GET /api/metrics - Complete metrics overview (all categories)
     * - GET /api/metrics/network - Network-specific metrics
     * - GET /api/metrics/performance - Processing performance metrics
     * - GET /api/metrics/resources - System resource utilization
     * - GET /api/metrics/errors - Error tracking and history
     * - GET /api/metrics/historical - Time-series historical data
     * - POST /api/metrics/reset - Reset all metric counters
     *
     * @par Query Parameters
     * - time_window: Time window in seconds (default: 300)
     * - format: Response format (json, csv, prometheus)
     * - include_historical: Include historical data (default: false)
     * - component_filter: Filter by component name
     *
     * @par Caching
     * The overview endpoint uses a 5-second cache to reduce load on the
     * MetricsCollector for high-frequency polling scenarios.
     *
     * @see MetricsCollector, BridgeMetrics, NetworkMetrics
     */
    class MetricsController
    {
    public:
        /**
         * @brief Constructs a metrics controller
         *
         * Initializes the controller and obtains a reference to the singleton
         * MetricsCollector instance.
         */
        MetricsController();

        /**
         * @brief Registers all metrics routes with the Crow application
         *
         * @param app Reference to the Crow SimpleApp instance
         */
        void registerRoutes(crow::SimpleApp &app);

        /**
         * @brief Returns complete metrics overview (GET /api/metrics)
         *
         * Provides a comprehensive snapshot of all metrics categories.
         * Results are cached for 5 seconds to reduce overhead.
         *
         * @param req HTTP request object (supports query parameters)
         * @return HTTP response with JSON metrics data
         *
         * @par Response Format
         * @code
         * {
         *   "network": {...},
         *   "processing": {...},
         *   "resources": {...},
         *   "errors": {...},
         *   "timestamp": "2025-01-24T12:00:00Z"
         * }
         * @endcode
         */
        crow::response getMetricsOverview(const crow::request &req);

        /**
         * @brief Returns network metrics (GET /api/metrics/network)
         *
         * Provides detailed network statistics including packet counts,
         * throughput, latency, and dropped packet information.
         *
         * @param req HTTP request object
         * @return HTTP response with network metrics JSON
         */
        crow::response getNetworkMetrics(const crow::request &req);

        /**
         * @brief Returns processing performance metrics (GET /api/metrics/performance)
         *
         * Provides encoding/decoding timing statistics, PDU processing rates,
         * and performance histograms.
         *
         * @param req HTTP request object
         * @return HTTP response with performance metrics JSON
         */
        crow::response getPerformanceMetrics(const crow::request &req);

        /**
         * @brief Returns system resource metrics (GET /api/metrics/resources)
         *
         * Provides CPU usage, memory consumption, thread counts, and queue depth
         * information.
         *
         * @param req HTTP request object
         * @return HTTP response with resource metrics JSON
         */
        crow::response getResourceMetrics(const crow::request &req);

        /**
         * @brief Returns error tracking metrics (GET /api/metrics/errors)
         *
         * Provides error/warning counts and recent error history by component.
         *
         * @param req HTTP request object
         * @return HTTP response with error metrics JSON
         */
        crow::response getErrorMetrics(const crow::request &req);

        /**
         * @brief Returns historical metrics data (GET /api/metrics/historical)
         *
         * Provides time-series metrics data for trending and analysis.
         *
         * @param req HTTP request object (time_window query param)
         * @return HTTP response with historical metrics JSON
         */
        crow::response getHistoricalMetrics(const crow::request &req);

        /**
         * @brief Resets all metric counters (POST /api/metrics/reset)
         *
         * Clears accumulated counters and statistics. Useful for benchmark
         * scenarios or after configuration changes.
         *
         * @param req HTTP request object
         * @return HTTP response indicating success
         *
         * @warning This operation cannot be undone
         */
        crow::response resetMetrics(const crow::request &req);

    private:
        Services::MetricsCollector &metrics_collector_;  ///< Reference to metrics collection service

        /**
         * @brief Formats network metrics as JSON
         */
        crow::json::wvalue formatNetworkMetrics(const Models::NetworkMetrics &network) const;

        /**
         * @brief Formats processing metrics as JSON
         */
        crow::json::wvalue formatProcessingMetrics(const Models::ProcessingMetrics &processing) const;

        /**
         * @brief Formats resource metrics as JSON
         */
        crow::json::wvalue formatResourceMetrics(const Models::ResourceMetrics &resources) const;

        /**
         * @brief Formats error metrics as JSON
         */
        crow::json::wvalue formatErrorMetrics(const Models::ErrorMetrics &errors) const;

        /**
         * @struct MetricsQuery
         * @brief Parsed query parameters for metrics requests
         */
        struct MetricsQuery
        {
            std::chrono::seconds time_window{300}; ///< Time window for historical data (default: 5 min)
            std::string format{"json"};            ///< Response format (json, csv, prometheus)
            bool include_historical{false};        ///< Whether to include historical data
            std::string component_filter{""};      ///< Filter by component name
        };

        /**
         * @brief Parses query parameters from HTTP request
         */
        MetricsQuery parseQuery(const crow::request &req) const;

        // Cache management for high-frequency polling
        mutable std::mutex cache_mutex_;                                  ///< Mutex protecting cache
        mutable std::chrono::steady_clock::time_point last_cache_time_;   ///< Last cache update time
        mutable std::string cached_overview_;                             ///< Cached overview response
        static constexpr std::chrono::seconds CACHE_DURATION{5};          ///< Cache validity duration

        /**
         * @brief Checks if cached data is still valid
         */
        bool isCacheValid() const;

        /**
         * @brief Updates the cache with new data
         */
        void updateCache(const std::string &data) const;
    };

    /**
     * @class HealthController
     * @brief HTTP REST controller for system health monitoring
     *
     * This controller provides endpoints for querying system and component health
     * status. It integrates with the MetricsCollector to assess overall system
     * health based on configurable thresholds.
     *
     * Health monitoring covers:
     * - SimConnect connection status
     * - DIS network connectivity
     * - REST API responsiveness
     * - Bridge core processing
     * - System resource availability
     *
     * @par HTTP Endpoints
     * - GET /api/health - Overall system health status
     * - GET /api/health/component/{name} - Specific component health
     * - POST /api/health/check - Trigger on-demand health check
     * - GET /api/health/history - Historical health status
     *
     * @par Health Status Values
     * - HEALTHY: All systems operating normally
     * - WARNING: Degraded performance or approaching limits
     * - CRITICAL: Component failure or severe issues
     * - UNKNOWN: Unable to determine health status
     *
     * @see MetricsCollector, SystemHealth, ComponentHealth, HealthStatus
     */
    class HealthController
    {
    public:
        /**
         * @brief Constructs a health controller
         */
        HealthController();

        /**
         * @brief Registers all health routes with the Crow application
         *
         * @param app Reference to the Crow SimpleApp instance
         */
        void registerRoutes(crow::SimpleApp &app);

        /**
         * @brief Returns overall system health (GET /api/health)
         *
         * Provides aggregated health status across all components.
         *
         * @param req HTTP request object
         * @return HTTP response with system health JSON
         *
         * @par Response Format
         * @code
         * {
         *   "status": "HEALTHY",
         *   "components": {
         *     "simconnect": {"status": "HEALTHY", "message": ""},
         *     "dis_network": {"status": "HEALTHY", "message": ""},
         *     ...
         *   },
         *   "timestamp": "2025-01-24T12:00:00Z"
         * }
         * @endcode
         */
        crow::response getHealthOverview(const crow::request &req);

        /**
         * @brief Returns health status of a specific component (GET /api/health/component/{name})
         *
         * @param req HTTP request object
         * @param component Component name (simconnect, dis_network, rest_api, bridge_core, system_resources)
         * @return HTTP response with component health JSON or 404 if invalid component
         */
        crow::response getComponentHealth(const crow::request &req, const std::string &component);

        /**
         * @brief Triggers an on-demand health check (POST /api/health/check)
         *
         * Performs immediate health assessment across all components.
         *
         * @param req HTTP request object
         * @return HTTP response with updated health status
         */
        crow::response performHealthCheck(const crow::request &req);

        /**
         * @brief Returns historical health status (GET /api/health/history)
         *
         * Provides time-series health status data for trend analysis.
         *
         * @param req HTTP request object (time_window query param)
         * @return HTTP response with historical health JSON
         */
        crow::response getHealthHistory(const crow::request &req);

    private:
        Services::MetricsCollector &metrics_collector_;  ///< Reference to metrics/health service

        /**
         * @brief Formats system health as JSON
         */
        crow::json::wvalue formatSystemHealth(const Models::SystemHealth &health) const;

        /**
         * @brief Formats component health as JSON
         */
        crow::json::wvalue formatComponentHealth(const Models::ComponentHealth &component) const;

        /**
         * @brief Triggers a comprehensive health check across all components
         */
        void triggerHealthCheck();

        /**
         * @brief Determines overall system health from component statuses
         */
        Models::HealthStatus determineOverallHealth(const Models::SystemHealth &health) const;

        /**
         * @brief Validates component name against known components
         */
        bool isValidComponentName(const std::string &component) const;

        /**
         * @brief Parses component name string to ComponentType enum
         */
        Models::ComponentType parseComponentType(const std::string &component) const;
    };

}