#include "MetricsController.h"

// Include the actual headers only in the .cpp file
#include "../models/MetricsModel.h"
#include "../models/HealthModel.h"
#include "../services/MetricsCollector.h"
#include "../utils/JsonResponse.h"

#include <sstream>
#include <iomanip>

namespace DISBridge::Controllers
{

    MetricsController::MetricsController()
        : metrics_collector_(Services::MetricsCollector::getInstance())
    {
    }

    void MetricsController::registerRoutes(crow::SimpleApp &app)
    {
        // Overview endpoint - most commonly used
        CROW_ROUTE(app, "/api/metrics")
            .methods("GET"_method)([this](const crow::request &req)
                                   { return getMetricsOverview(req); });

        // Specific metric categories
        CROW_ROUTE(app, "/api/metrics/network")
            .methods("GET"_method)([this](const crow::request &req)
                                   { return getNetworkMetrics(req); });

        CROW_ROUTE(app, "/api/metrics/performance")
            .methods("GET"_method)([this](const crow::request &req)
                                   { return getPerformanceMetrics(req); });

        CROW_ROUTE(app, "/api/metrics/resources")
            .methods("GET"_method)([this](const crow::request &req)
                                   { return getResourceMetrics(req); });

        CROW_ROUTE(app, "/api/metrics/errors")
            .methods("GET"_method)([this](const crow::request &req)
                                   { return getErrorMetrics(req); });

        // Historical data endpoint
        CROW_ROUTE(app, "/api/metrics/historical")
            .methods("GET"_method)([this](const crow::request &req)
                                   { return getHistoricalMetrics(req); });

        // Reset metrics endpoint (POST for safety)
        CROW_ROUTE(app, "/api/metrics/reset")
            .methods("POST"_method)([this](const crow::request &req)
                                    { return resetMetrics(req); });
    }

    crow::response MetricsController::getMetricsOverview(const crow::request &req)
    {
        try
        {
            // Check cache first
            {
                std::lock_guard<std::mutex> lock(cache_mutex_);
                if (isCacheValid() && !cached_overview_.empty())
                {
                    crow::response res;
                    res.code = 200;
                    res.set_header("Content-Type", "application/json");
                    res.body = cached_overview_;
                    return res;
                }
            }

            auto metrics = metrics_collector_.getMetricsSnapshot();
            auto query = parseQuery(req);

            crow::json::wvalue response;
            response["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                                        std::chrono::system_clock::now().time_since_epoch())
                                        .count();

            response["uptime_seconds"] = metrics.getUptime().count();

            // Network summary
            response["network"]["packets_sent"] = metrics.network.total_packets_sent.load();
            response["network"]["packets_received"] = metrics.network.total_packets_received.load();
            response["network"]["bytes_sent"] = metrics.network.total_bytes_sent.load();
            response["network"]["bytes_received"] = metrics.network.total_bytes_received.load();
            response["network"]["dropped_packets"] = metrics.network.dropped_packets.load();

            {
                std::lock_guard<std::mutex> lock(metrics.network.metrics_mutex);
                response["network"]["avg_latency_ms"] = metrics.network.avg_latency_ms;
                response["network"]["packets_per_second"] = metrics.network.packets_per_second;
            }

            // Processing summary
            response["processing"]["pdus_encoded"] = metrics.processing.pdus_encoded.load();
            response["processing"]["pdus_decoded"] = metrics.processing.pdus_decoded.load();
            response["processing"]["encoding_failures"] = metrics.processing.encoding_failures.load();
            response["processing"]["decoding_failures"] = metrics.processing.decoding_failures.load();

            {
                std::lock_guard<std::mutex> lock(metrics.processing.timing_mutex);
                response["processing"]["avg_encoding_time_ms"] = metrics.processing.avg_encoding_time_ms;
                response["processing"]["avg_decoding_time_ms"] = metrics.processing.avg_decoding_time_ms;
            }

            // Resource summary
            response["resources"]["cpu_usage_percent"] = metrics.resources.cpu_usage_percent.load();
            response["resources"]["memory_usage_mb"] = metrics.resources.memory_usage_mb.load();
            response["resources"]["active_threads"] = metrics.resources.active_threads.load();
            response["resources"]["queue_depth"] = metrics.resources.queue_depth.load();

            // Error summary
            response["errors"]["total_errors"] = metrics.errors.total_errors.load();
            response["errors"]["total_warnings"] = metrics.errors.total_warnings.load();
            response["errors"]["critical_errors"] = metrics.errors.critical_errors.load();

            // SimConnect status
            response["simconnect"]["is_connected"] = metrics.simconnect.is_connected.load();
            response["simconnect"]["successful_connections"] = metrics.simconnect.successful_connections.load();
            response["simconnect"]["connection_failures"] = metrics.simconnect.connection_failures.load();

            std::string json_str = response.dump();
            updateCache(json_str);

            crow::response res;
            res.code = 200;
            res.set_header("Content-Type", "application/json");
            res.body = json_str;
            return res;
        }
        catch (const std::exception &e)
        {
            crow::response res;
            res.code = 500;
            res.set_header("Content-Type", "application/json");

            crow::json::wvalue error_response;
            error_response["error"] = "Failed to retrieve metrics";
            error_response["details"] = e.what();
            res.body = error_response.dump();
            return res;
        }
    }

    crow::response MetricsController::getNetworkMetrics(const crow::request &req)
    {
        try
        {
            auto metrics = metrics_collector_.getMetricsSnapshot();
            auto network_json = formatNetworkMetrics(metrics.network);

            crow::response res;
            res.code = 200;
            res.set_header("Content-Type", "application/json");
            res.body = network_json.dump();
            return res;
        }
        catch (const std::exception &e)
        {
            crow::response res;
            res.code = 500;
            res.set_header("Content-Type", "application/json");

            crow::json::wvalue error_response;
            error_response["error"] = "Failed to retrieve network metrics";
            error_response["details"] = e.what();
            res.body = error_response.dump();
            return res;
        }
    }

    crow::response MetricsController::getPerformanceMetrics(const crow::request &req)
    {
        try
        {
            auto metrics = metrics_collector_.getMetricsSnapshot();
            auto processing_json = formatProcessingMetrics(metrics.processing);

            crow::response res;
            res.code = 200;
            res.set_header("Content-Type", "application/json");
            res.body = processing_json.dump();
            return res;
        }
        catch (const std::exception &e)
        {
            crow::response res;
            res.code = 500;
            res.set_header("Content-Type", "application/json");

            crow::json::wvalue error_response;
            error_response["error"] = "Failed to retrieve performance metrics";
            error_response["details"] = e.what();
            res.body = error_response.dump();
            return res;
        }
    }

    crow::response MetricsController::getResourceMetrics(const crow::request &req)
    {
        try
        {
            auto metrics = metrics_collector_.getMetricsSnapshot();
            auto resources_json = formatResourceMetrics(metrics.resources);

            crow::response res;
            res.code = 200;
            res.set_header("Content-Type", "application/json");
            res.body = resources_json.dump();
            return res;
        }
        catch (const std::exception &e)
        {
            crow::response res;
            res.code = 500;
            res.set_header("Content-Type", "application/json");

            crow::json::wvalue error_response;
            error_response["error"] = "Failed to retrieve resource metrics";
            error_response["details"] = e.what();
            res.body = error_response.dump();
            return res;
        }
    }

    crow::response MetricsController::getErrorMetrics(const crow::request &req)
    {
        try
        {
            auto metrics = metrics_collector_.getMetricsSnapshot();
            auto errors_json = formatErrorMetrics(metrics.errors);

            crow::response res;
            res.code = 200;
            res.set_header("Content-Type", "application/json");
            res.body = errors_json.dump();
            return res;
        }
        catch (const std::exception &e)
        {
            crow::response res;
            res.code = 500;
            res.set_header("Content-Type", "application/json");

            crow::json::wvalue error_response;
            error_response["error"] = "Failed to retrieve error metrics";
            error_response["details"] = e.what();
            res.body = error_response.dump();
            return res;
        }
    }

    crow::response MetricsController::getHistoricalMetrics(const crow::request &req)
    {
        try
        {
            crow::json::wvalue response;
            response["message"] = "Historical metrics not yet implemented";
            response["available"] = false;

            crow::response res;
            res.code = 200;
            res.set_header("Content-Type", "application/json");
            res.body = response.dump();
            return res;
        }
        catch (const std::exception &e)
        {
            crow::response res;
            res.code = 500;
            res.set_header("Content-Type", "application/json");

            crow::json::wvalue error_response;
            error_response["error"] = "Failed to retrieve historical metrics";
            error_response["details"] = e.what();
            res.body = error_response.dump();
            return res;
        }
    }

    crow::response MetricsController::resetMetrics(const crow::request &req)
    {
        try
        {
            // This would need to be implemented in MetricsCollector
            // metrics_collector_.resetCounters();

            crow::json::wvalue response;
            response["message"] = "Metrics reset functionality not yet implemented";
            response["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                                        std::chrono::system_clock::now().time_since_epoch())
                                        .count();

            crow::response res;
            res.code = 200;
            res.set_header("Content-Type", "application/json");
            res.body = response.dump();
            return res;
        }
        catch (const std::exception &e)
        {
            crow::response res;
            res.code = 500;
            res.set_header("Content-Type", "application/json");

            crow::json::wvalue error_response;
            error_response["error"] = "Failed to reset metrics";
            error_response["details"] = e.what();
            res.body = error_response.dump();
            return res;
        }
    }

    // Helper method implementations
    MetricsController::MetricsQuery MetricsController::parseQuery(const crow::request &req) const
    {
        MetricsQuery query;

        // Parse time window
        auto time_param = req.url_params.get("time_window");
        if (time_param)
        {
            try
            {
                int seconds = std::stoi(time_param);
                query.time_window = std::chrono::seconds(std::max(1, std::min(3600, seconds))); // 1 sec to 1 hour
            }
            catch (...)
            {
                // Use default
            }
        }

        // Parse format
        auto format_param = req.url_params.get("format");
        if (format_param)
        {
            query.format = format_param;
        }

        // Parse historical flag
        auto hist_param = req.url_params.get("include_historical");
        if (hist_param)
        {
            query.include_historical = (std::string(hist_param) == "true");
        }

        return query;
    }

    crow::json::wvalue MetricsController::formatNetworkMetrics(const Models::NetworkMetrics &network) const
    {
        crow::json::wvalue json;

        json["total_packets_sent"] = network.total_packets_sent.load();
        json["total_packets_received"] = network.total_packets_received.load();
        json["total_bytes_sent"] = network.total_bytes_sent.load();
        json["total_bytes_received"] = network.total_bytes_received.load();
        json["dropped_packets"] = network.dropped_packets.load();
        json["failed_transmissions"] = network.failed_transmissions.load();

        {
            std::lock_guard<std::mutex> lock(network.metrics_mutex);
            json["avg_latency_ms"] = network.avg_latency_ms;
            json["packets_per_second"] = network.packets_per_second;
            json["bytes_per_second"] = network.bytes_per_second;
        }

        return json;
    }

    crow::json::wvalue MetricsController::formatProcessingMetrics(const Models::ProcessingMetrics &processing) const
    {
        crow::json::wvalue json;

        json["pdus_encoded"] = processing.pdus_encoded.load();
        json["pdus_decoded"] = processing.pdus_decoded.load();
        json["encoding_failures"] = processing.encoding_failures.load();
        json["decoding_failures"] = processing.decoding_failures.load();

        {
            std::lock_guard<std::mutex> lock(processing.timing_mutex);
            json["avg_encoding_time_ms"] = processing.avg_encoding_time_ms;
            json["avg_decoding_time_ms"] = processing.avg_decoding_time_ms;
            json["avg_round_trip_time_ms"] = processing.avg_round_trip_time_ms;
        }

        return json;
    }

    crow::json::wvalue MetricsController::formatResourceMetrics(const Models::ResourceMetrics &resources) const
    {
        crow::json::wvalue json;

        json["cpu_usage_percent"] = resources.cpu_usage_percent.load();
        json["memory_usage_mb"] = resources.memory_usage_mb.load();
        json["active_threads"] = resources.active_threads.load();
        json["queue_depth"] = resources.queue_depth.load();

        {
            std::lock_guard<std::mutex> lock(resources.uptime_mutex);
            json["uptime_seconds"] = resources.uptime.count();
        }

        return json;
    }

    crow::json::wvalue MetricsController::formatErrorMetrics(const Models::ErrorMetrics &errors) const
    {
        crow::json::wvalue json;

        json["total_errors"] = errors.total_errors.load();
        json["total_warnings"] = errors.total_warnings.load();
        json["critical_errors"] = errors.critical_errors.load();

        // Add recent errors (limited for performance)
        std::lock_guard<std::mutex> lock(errors.recent_errors_mutex);
        crow::json::wvalue recent_errors(crow::json::type::List);

        size_t count = 0;
        for (const auto &error_entry : errors.recent_errors)
        {
            if (count >= 10)
                break; // Limit to 10 most recent errors

            crow::json::wvalue error_json;
            error_json["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                                          error_entry.first.time_since_epoch())
                                          .count();
            error_json["message"] = error_entry.second;
            recent_errors[count++] = std::move(error_json);
        }

        json["recent_errors"] = std::move(recent_errors);

        return json;
    }

    bool MetricsController::isCacheValid() const
    {
        auto now = std::chrono::steady_clock::now();
        return (now - last_cache_time_) < CACHE_DURATION;
    }

    void MetricsController::updateCache(const std::string &data) const
    {
        cached_overview_ = data;
        last_cache_time_ = std::chrono::steady_clock::now();
    }

    HealthController::HealthController()
        : metrics_collector_(Services::MetricsCollector::getInstance())
    {
    }

    void HealthController::registerRoutes(crow::SimpleApp& app)
    {
        // GET /api/health - Overall system health
        CROW_ROUTE(app, "/api/health")
            .methods("GET"_method)
            ([this](const crow::request& req) {
                return this->getHealthOverview(req);
            });

        // GET /api/health/:component - Specific component health
        CROW_ROUTE(app, "/api/health/<string>")
            .methods("GET"_method)
            ([this](const crow::request& req, const std::string& component) {
                return this->getComponentHealth(req, component);
            });

        // POST /api/health/check - Trigger health check
        CROW_ROUTE(app, "/api/health/check")
            .methods("POST"_method)
            ([this](const crow::request& req) {
                return this->performHealthCheck(req);
            });

        // GET /api/health/history - Health check history
        CROW_ROUTE(app, "/api/health/history")
            .methods("GET"_method)
            ([this](const crow::request& req) {
                return this->getHealthHistory(req);
            });
    }

    crow::response HealthController::getHealthOverview(const crow::request& req)
    {
        try
        {
            auto health = metrics_collector_.getHealthSnapshot();
            auto data = formatSystemHealth(health);

            crow::response res;
            res.code = 200;
            res.set_header("Content-Type", "application/json");
            res.body = data.dump();
            return res;
        }
        catch (const std::exception& e)
        {
            crow::response res;
            res.code = 500;
            res.set_header("Content-Type", "application/json");

            crow::json::wvalue error_response;
            error_response["error"] = "Failed to retrieve health status";
            error_response["details"] = e.what();
            res.body = error_response.dump();
            return res;
        }
    }

    crow::response HealthController::getComponentHealth(const crow::request& req, const std::string& component)
    {
        try
        {
            if (!isValidComponentName(component))
            {
                crow::response res;
                res.code = 400;
                res.set_header("Content-Type", "application/json");

                crow::json::wvalue error_response;
                error_response["error"] = "Invalid component name";
                error_response["component"] = component;
                res.body = error_response.dump();
                return res;
            }

            auto health = metrics_collector_.getHealthSnapshot();
            auto component_type = parseComponentType(component);

            // Find the component
            for (const auto& comp : health.components)
            {
                if (comp.component == component_type)
                {
                    auto data = formatComponentHealth(comp);

                    crow::response res;
                    res.code = 200;
                    res.set_header("Content-Type", "application/json");
                    res.body = data.dump();
                    return res;
                }
            }

            crow::response res;
            res.code = 404;
            res.set_header("Content-Type", "application/json");

            crow::json::wvalue error_response;
            error_response["error"] = "Component not found";
            error_response["component"] = component;
            res.body = error_response.dump();
            return res;
        }
        catch (const std::exception& e)
        {
            crow::response res;
            res.code = 500;
            res.set_header("Content-Type", "application/json");

            crow::json::wvalue error_response;
            error_response["error"] = "Failed to retrieve component health";
            error_response["details"] = e.what();
            res.body = error_response.dump();
            return res;
        }
    }

    crow::response HealthController::performHealthCheck(const crow::request& req)
    {
        try
        {
            triggerHealthCheck();

            crow::json::wvalue data;
            data["message"] = "Health check triggered";
            data["status"] = "completed";
            data["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            crow::response res;
            res.code = 200;
            res.set_header("Content-Type", "application/json");
            res.body = data.dump();
            return res;
        }
        catch (const std::exception& e)
        {
            crow::response res;
            res.code = 500;
            res.set_header("Content-Type", "application/json");

            crow::json::wvalue error_response;
            error_response["error"] = "Failed to perform health check";
            error_response["details"] = e.what();
            res.body = error_response.dump();
            return res;
        }
    }

    crow::response HealthController::getHealthHistory(const crow::request& req)
    {
        try
        {
            crow::json::wvalue data;
            data["message"] = "Health history endpoint";
            data["note"] = "Historical health data storage not yet implemented";

            crow::response res;
            res.code = 200;
            res.set_header("Content-Type", "application/json");
            res.body = data.dump();
            return res;
        }
        catch (const std::exception& e)
        {
            crow::response res;
            res.code = 500;
            res.set_header("Content-Type", "application/json");

            crow::json::wvalue error_response;
            error_response["error"] = "Failed to retrieve health history";
            error_response["details"] = e.what();
            res.body = error_response.dump();
            return res;
        }
    }

    crow::json::wvalue HealthController::formatSystemHealth(const Models::SystemHealth& health) const
    {
        crow::json::wvalue data;

        data["overall_status"] = Models::healthStatusToString(health.overall_status);
        data["version"] = health.version;
        data["uptime_seconds"] = health.uptime.count();

        // Format components
        crow::json::wvalue components_array(crow::json::type::List);
        size_t index = 0;
        for (const auto& comp : health.components)
        {
            components_array[index++] = formatComponentHealth(comp);
        }
        data["components"] = std::move(components_array);

        return data;
    }

    crow::json::wvalue HealthController::formatComponentHealth(const Models::ComponentHealth& component) const
    {
        crow::json::wvalue data;

        data["component"] = Models::componentTypeToString(component.component);
        data["status"] = Models::healthStatusToString(component.status);
        data["message"] = component.message;

        // Add details if present
        if (!component.details.empty())
        {
            crow::json::wvalue details;
            for (const auto& [key, value] : component.details)
            {
                details[key] = value;
            }
            data["details"] = std::move(details);
        }

        return data;
    }

    void HealthController::triggerHealthCheck()
    {
        // force immediate health update
        // MetricsCollector's monitoring loop handles this automatically
        // placeholder for future health check implementation
    }

    Models::HealthStatus HealthController::determineOverallHealth(const Models::SystemHealth& health) const
    {
        return health.overall_status;
    }

    bool HealthController::isValidComponentName(const std::string& component) const
    {
        return component == "simconnect" ||
               component == "dis_network" ||
               component == "rest_api" ||
               component == "bridge_core" ||
               component == "system_resources";
    }

    Models::ComponentType HealthController::parseComponentType(const std::string& component) const
    {
        if (component == "simconnect")
            return Models::ComponentType::SIMCONNECT;
        else if (component == "dis_network")
            return Models::ComponentType::DIS_NETWORK;
        else if (component == "rest_api")
            return Models::ComponentType::REST_API;
        else if (component == "bridge_core")
            return Models::ComponentType::BRIDGE_CORE;
        else if (component == "system_resources")
            return Models::ComponentType::SYSTEM_RESOURCES;
        else
            return Models::ComponentType::BRIDGE_CORE; // Default fallback
    }

}