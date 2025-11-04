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

    class MetricsController
    {
    public:
        MetricsController();

        // Register all metrics routes with the Crow app
        void registerRoutes(crow::SimpleApp &app);

        // Individual endpoint handlers
        crow::response getMetricsOverview(const crow::request &req);
        crow::response getNetworkMetrics(const crow::request &req);
        crow::response getPerformanceMetrics(const crow::request &req);
        crow::response getResourceMetrics(const crow::request &req);
        crow::response getErrorMetrics(const crow::request &req);
        crow::response getHistoricalMetrics(const crow::request &req);
        crow::response resetMetrics(const crow::request &req);

    private:
        Services::MetricsCollector &metrics_collector_;

        // Helper methods for data formatting
        crow::json::wvalue formatNetworkMetrics(const Models::NetworkMetrics &network) const;
        crow::json::wvalue formatProcessingMetrics(const Models::ProcessingMetrics &processing) const;
        crow::json::wvalue formatResourceMetrics(const Models::ResourceMetrics &resources) const;
        crow::json::wvalue formatErrorMetrics(const Models::ErrorMetrics &errors) const;

        // Query parameter parsing
        struct MetricsQuery
        {
            std::chrono::seconds time_window{300}; // Default 5 minutes
            std::string format{"json"};
            bool include_historical{false};
            std::string component_filter{""};
        };

        MetricsQuery parseQuery(const crow::request &req) const;

        // Rate limiting and caching
        mutable std::mutex cache_mutex_;
        mutable std::chrono::steady_clock::time_point last_cache_time_;
        mutable std::string cached_overview_;
        static constexpr std::chrono::seconds CACHE_DURATION{5};

        bool isCacheValid() const;
        void updateCache(const std::string &data) const;
    };

    class HealthController
    {
    public:
        HealthController();

        // Register all health routes with the Crow app
        void registerRoutes(crow::SimpleApp &app);

        // Individual endpoint handlers
        crow::response getHealthOverview(const crow::request &req);
        crow::response getComponentHealth(const crow::request &req, const std::string &component);
        crow::response performHealthCheck(const crow::request &req);
        crow::response getHealthHistory(const crow::request &req);

    private:
        Services::MetricsCollector &metrics_collector_;

        // Helper methods
        crow::json::wvalue formatSystemHealth(const Models::SystemHealth &health) const;
        crow::json::wvalue formatComponentHealth(const Models::ComponentHealth &component) const;

        // Health check operations
        void triggerHealthCheck();
        Models::HealthStatus determineOverallHealth(const Models::SystemHealth &health) const;

        // Component name validation
        bool isValidComponentName(const std::string &component) const;
        Models::ComponentType parseComponentType(const std::string &component) const;
    };

}