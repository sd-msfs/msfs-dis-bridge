#include "HealthModel.h"
#include <crow.h>
#include <fstream>
#include <algorithm>

namespace DISBridge::Models
{

    void SystemHealth::calculateOverallStatus()
    {
        if (components.empty())
        {
            overall_status = HealthStatus::UNKNOWN;
            return;
        }

        // Determine overall status based on component statuses
        // Priority: CRITICAL > WARNING > HEALTHY > UNKNOWN
        bool has_critical = false;
        bool has_warning = false;
        bool has_healthy = false;

        for (const auto& component : components)
        {
            switch (component.status)
            {
            case HealthStatus::CRITICAL:
                has_critical = true;
                break;
            case HealthStatus::WARNING:
                has_warning = true;
                break;
            case HealthStatus::HEALTHY:
                has_healthy = true;
                break;
            case HealthStatus::UNKNOWN:
                break;
            }
        }

        if (has_critical)
        {
            overall_status = HealthStatus::CRITICAL;
        }
        else if (has_warning)
        {
            overall_status = HealthStatus::WARNING;
        }
        else if (has_healthy)
        {
            overall_status = HealthStatus::HEALTHY;
        }
        else
        {
            overall_status = HealthStatus::UNKNOWN;
        }

        timestamp = std::chrono::steady_clock::now();
    }

    std::string SystemHealth::toJson() const
    {
        crow::json::wvalue json;

        json["overall_status"] = healthStatusToString(overall_status);
        json["version"] = version;
        json["uptime_seconds"] = uptime.count();

        // Timestamp as ISO 8601
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm tm_now;
        gmtime_s(&tm_now, &time_t_now);

        char timestamp_buffer[32];
        std::strftime(timestamp_buffer, sizeof(timestamp_buffer), "%Y-%m-%dT%H:%M:%SZ", &tm_now);
        json["timestamp"] = timestamp_buffer;

        // Component health
        std::vector<crow::json::wvalue> component_array;
        for (const auto& comp : components)
        {
            crow::json::wvalue comp_json;
            comp_json["component"] = componentTypeToString(comp.component);
            comp_json["status"] = healthStatusToString(comp.status);
            comp_json["message"] = comp.message;

            // Add details if any
            if (!comp.details.empty())
            {
                crow::json::wvalue details_json;
                for (const auto& [key, value] : comp.details)
                {
                    details_json[key] = value;
                }
                comp_json["details"] = std::move(details_json);
            }

            component_array.push_back(std::move(comp_json));
        }
        json["components"] = std::move(component_array);

        return json.dump();
    }

    void SystemHealth::updateComponent(ComponentType component, HealthStatus status,
                                       const std::string& message,
                                       const std::unordered_map<std::string, std::string>& details)
    {
        // Find existing component or add new one
        auto it = std::find_if(components.begin(), components.end(),
            [component](const ComponentHealth& c) { return c.component == component; });

        if (it != components.end())
        {
            // Update existing component
            it->status = status;
            it->message = message;
            it->details = details;
            it->last_check = std::chrono::steady_clock::now();
        }
        else
        {
            // Add new component
            ComponentHealth new_component(component, status);
            new_component.message = message;
            new_component.details = details;
            components.push_back(new_component);
        }

        // Recalculate overall status
        calculateOverallStatus();
    }

    void HealthThresholds::loadFromConfig(const std::string& config_path)
    {
        if (config_path.empty())
        {
            return;
        }

        std::ifstream config_file(config_path);
        if (!config_file.is_open())
        {
            // Use default values if config file doesn't exist
            return;
        }

        // Simple key-value parser (switch to JSON/INI parser in future prod)
        std::string line;
        while (std::getline(config_file, line))
        {
            if (line.empty() || line[0] == '#')
                continue;

            size_t delimiter_pos = line.find('=');
            if (delimiter_pos == std::string::npos)
                continue;

            std::string key = line.substr(0, delimiter_pos);
            std::string value = line.substr(delimiter_pos + 1);

            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            try
            {
                if (key == "max_cpu_percent")
                    max_cpu_percent = std::stod(value);
                else if (key == "max_memory_mb")
                    max_memory_mb = std::stoull(value);
                else if (key == "max_queue_depth")
                    max_queue_depth = std::stoul(value);
                else if (key == "max_avg_latency_ms")
                    max_avg_latency_ms = std::stod(value);
                else if (key == "max_consecutive_failures")
                    max_consecutive_failures = std::stoull(value);
                else if (key == "component_timeout_seconds")
                    component_timeout = std::chrono::seconds(std::stoi(value));
            }
            catch (const std::exception&)
            {
                continue;
            }
        }
    }

    std::string healthStatusToString(HealthStatus status)
    {
        switch (status)
        {
        case HealthStatus::HEALTHY:
            return "healthy";
        case HealthStatus::WARNING:
            return "warning";
        case HealthStatus::CRITICAL:
            return "critical";
        case HealthStatus::UNKNOWN:
        default:
            return "unknown";
        }
    }

    std::string componentTypeToString(ComponentType type)
    {
        switch (type)
        {
        case ComponentType::SIMCONNECT:
            return "simconnect";
        case ComponentType::DIS_NETWORK:
            return "dis_network";
        case ComponentType::REST_API:
            return "rest_api";
        case ComponentType::BRIDGE_CORE:
            return "bridge_core";
        case ComponentType::SYSTEM_RESOURCES:
            return "system_resources";
        default:
            return "unknown";
        }
    }

    HealthStatus stringToHealthStatus(const std::string& status_str)
    {
        std::string lower_status = status_str;
        std::transform(lower_status.begin(), lower_status.end(), lower_status.begin(),
            [](unsigned char c) { return std::tolower(c); });

        if (lower_status == "healthy")
            return HealthStatus::HEALTHY;
        else if (lower_status == "warning")
            return HealthStatus::WARNING;
        else if (lower_status == "critical")
            return HealthStatus::CRITICAL;
        else
            return HealthStatus::UNKNOWN;
    }

}
