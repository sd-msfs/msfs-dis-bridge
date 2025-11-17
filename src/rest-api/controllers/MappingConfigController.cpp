#include "MappingConfigController.h"
#include "MappingConfig.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace DISBridge::Controllers
{

    MappingConfigController::MappingConfigController(MappingConfig& mappingConfig, const std::string& mappingsDir)
        : mappingConfig_(mappingConfig), mappingsDir_(mappingsDir)
    {
    }

    void MappingConfigController::registerRoutes(crow::SimpleApp &app)
    {
        // GET /api/mappings - Get all current mappings
        CROW_ROUTE(app, "/api/mappings")
            .methods("GET"_method)([this](const crow::request &req)
                                   { return getCurrentMappings(req); });

        // PUT /api/mappings - Update all mappings (reload from file or update entire config)
        CROW_ROUTE(app, "/api/mappings")
            .methods("PUT"_method)([this](const crow::request &req)
                                   { return updateMapping(req); });

        // POST /api/mappings/reload - Reload mappings from the currently loaded profile
        CROW_ROUTE(app, "/api/mappings/reload")
            .methods("POST"_method)([this](const crow::request &req)
                                    { return reloadMappings(req); });

        // GET /api/mappings/:eventName - Get a specific mapping entry
        CROW_ROUTE(app, "/api/mappings/<string>")
            .methods("GET"_method)([this](const crow::request &req, const std::string &eventName)
                                   { return getMappingEntry(req, eventName); });
    }

    crow::response MappingConfigController::getCurrentMappings(const crow::request &req)
    {
        try
        {
            std::lock_guard<std::mutex> lock(config_mutex_);

            const auto& mappings = mappingConfig_.getMappings();

            crow::json::wvalue response;
            response["count"] = static_cast<int>(mappings.size());

            std::vector<crow::json::wvalue> mappingArray;
            for (const auto& entry : mappings)
            {
                mappingArray.push_back(formatMappingEntry(entry));
            }
            response["mappings"] = std::move(mappingArray);

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
            error_response["error"] = "Failed to retrieve mappings";
            error_response["details"] = e.what();
            res.body = error_response.dump();
            return res;
        }
    }

    crow::response MappingConfigController::updateMapping(const crow::request &req)
    {
        try
        {
            auto json = crow::json::load(req.body);
            if (!json)
            {
                crow::response res;
                res.code = 400;
                res.set_header("Content-Type", "application/json");

                crow::json::wvalue error_response;
                error_response["error"] = "Invalid JSON in request body";
                res.body = error_response.dump();
                return res;
            }

            std::lock_guard<std::mutex> lock(config_mutex_);

            // Check if this is a request to load a profile
            if (json.has("profile"))
            {
                std::string profileName = json["profile"].s();

                // Validate profile name (prevent path traversal)
                if (profileName.find("..") != std::string::npos ||
                    profileName.find('/') != std::string::npos ||
                    profileName.find('\\') != std::string::npos)
                {
                    crow::response res;
                    res.code = 400;
                    res.set_header("Content-Type", "application/json");

                    crow::json::wvalue error_response;
                    error_response["error"] = "Invalid profile name";
                    res.body = error_response.dump();
                    return res;
                }

                std::filesystem::path profilePath = std::filesystem::path(mappingsDir_) / profileName;
                if (!std::filesystem::exists(profilePath))
                {
                    crow::response res;
                    res.code = 404;
                    res.set_header("Content-Type", "application/json");

                    crow::json::wvalue error_response;
                    error_response["error"] = "Profile not found";
                    error_response["profile"] = profileName;
                    res.body = error_response.dump();
                    return res;
                }

                if (!mappingConfig_.loadProfileFromCSV(profilePath.string()))
                {
                    crow::response res;
                    res.code = 500;
                    res.set_header("Content-Type", "application/json");

                    crow::json::wvalue error_response;
                    error_response["error"] = "Failed to load profile";
                    error_response["profile"] = profileName;
                    res.body = error_response.dump();
                    return res;
                }

                crow::json::wvalue success_response;
                success_response["status"] = "success";
                success_response["message"] = "Profile loaded successfully";
                success_response["profile"] = profileName;
                success_response["count"] = static_cast<int>(mappingConfig_.getMappings().size());

                crow::response res;
                res.code = 200;
                res.set_header("Content-Type", "application/json");
                res.body = success_response.dump();
                return res;
            }

            // If not loading a profile, reject for now (could implement runtime config updates later)
            crow::response res;
            res.code = 501;
            res.set_header("Content-Type", "application/json");

            crow::json::wvalue error_response;
            error_response["error"] = "Runtime mapping updates not yet implemented";
            error_response["message"] = "Use the 'profile' field to load a CSV profile";
            res.body = error_response.dump();
            return res;
        }
        catch (const std::exception &e)
        {
            crow::response res;
            res.code = 500;
            res.set_header("Content-Type", "application/json");

            crow::json::wvalue error_response;
            error_response["error"] = "Failed to update mappings";
            error_response["details"] = e.what();
            res.body = error_response.dump();
            return res;
        }
    }

    crow::response MappingConfigController::getMappingEntry(const crow::request &req, const std::string &eventName)
    {
        try
        {
            std::lock_guard<std::mutex> lock(config_mutex_);

            const MappingEntry* entry = mappingConfig_.getMapping(eventName);
            if (!entry)
            {
                crow::response res;
                res.code = 404;
                res.set_header("Content-Type", "application/json");

                crow::json::wvalue error_response;
                error_response["error"] = "Mapping entry not found";
                error_response["eventName"] = eventName;
                res.body = error_response.dump();
                return res;
            }

            crow::json::wvalue response = formatMappingEntry(*entry);

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
            error_response["error"] = "Failed to retrieve mapping entry";
            error_response["details"] = e.what();
            res.body = error_response.dump();
            return res;
        }
    }

    crow::response MappingConfigController::reloadMappings(const crow::request &req)
    {
        try
        {
            std::lock_guard<std::mutex> lock(config_mutex_);

            // Reload the default profile
            std::filesystem::path defaultProfile = std::filesystem::path(mappingsDir_) / "default-mapping.csv";

            if (!std::filesystem::exists(defaultProfile))
            {
                crow::response res;
                res.code = 404;
                res.set_header("Content-Type", "application/json");

                crow::json::wvalue error_response;
                error_response["error"] = "Default profile not found";
                error_response["path"] = defaultProfile.string();
                res.body = error_response.dump();
                return res;
            }

            if (!mappingConfig_.loadProfileFromCSV(defaultProfile.string()))
            {
                crow::response res;
                res.code = 500;
                res.set_header("Content-Type", "application/json");

                crow::json::wvalue error_response;
                error_response["error"] = "Failed to reload default profile";
                res.body = error_response.dump();
                return res;
            }

            crow::json::wvalue success_response;
            success_response["status"] = "success";
            success_response["message"] = "Mappings reloaded from default profile";
            success_response["count"] = static_cast<int>(mappingConfig_.getMappings().size());

            crow::response res;
            res.code = 200;
            res.set_header("Content-Type", "application/json");
            res.body = success_response.dump();
            return res;
        }
        catch (const std::exception &e)
        {
            crow::response res;
            res.code = 500;
            res.set_header("Content-Type", "application/json");

            crow::json::wvalue error_response;
            error_response["error"] = "Failed to reload mappings";
            error_response["details"] = e.what();
            res.body = error_response.dump();
            return res;
        }
    }

    bool MappingConfigController::validateMappingEntry(const crow::json::rvalue& entry, std::string& errorMsg) const
    {
        if (!entry.has("eventName") || !entry.has("pduType"))
        {
            errorMsg = "Missing required fields: eventName and pduType";
            return false;
        }

        // Add more validation as needed
        return true;
    }

    crow::json::wvalue MappingConfigController::formatMappingEntry(const MappingEntry& entry) const
    {
        crow::json::wvalue json;
        json["eventName"] = entry.eventName;
        json["fieldName"] = entry.fieldName;
        json["pduType"] = entry.pduType;
        json["pduField"] = entry.pduField;
        json["enabled"] = entry.enabled;
        json["rateHz"] = entry.rateHz;
        return json;
    }

}
