#pragma once

#include <crow.h>
#include <string>
#include <mutex>

// Forward declarations
class MappingConfig;
struct MappingEntry;

namespace DISBridge::Controllers
{

    class MappingConfigController
    {
    public:
        MappingConfigController(MappingConfig& mappingConfig, const std::string& mappingsDir);

        // Register all mapping config routes with the Crow app
        void registerRoutes(crow::SimpleApp &app);

        // Individual endpoint handlers
        crow::response getCurrentMappings(const crow::request &req);
        crow::response updateMapping(const crow::request &req);
        crow::response getMappingEntry(const crow::request &req, const std::string &eventName);
        crow::response reloadMappings(const crow::request &req);

    private:
        MappingConfig& mappingConfig_;
        std::string mappingsDir_;
        mutable std::mutex config_mutex_;

        // Helper methods
        bool validateMappingEntry(const crow::json::rvalue& entry, std::string& errorMsg) const;
        crow::json::wvalue formatMappingEntry(const MappingEntry& entry) const;
    };

}
