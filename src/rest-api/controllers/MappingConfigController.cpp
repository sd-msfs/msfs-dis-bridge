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

        // GET /api/mappings/profiles - Get list of available mapping profiles
        // POST /api/mappings/profiles - Create a new mapping profile
        CROW_ROUTE(app, "/api/mappings/profiles")
            .methods("GET"_method, "POST"_method)
            ([this](const crow::request &req) {
                if (req.method == crow::HTTPMethod::Get)
                {
                    return getAvailableProfiles(req);
                }
                else if (req.method == crow::HTTPMethod::Post)
                {
                    return createProfile(req);
                }
                else
                {
                    crow::response res;
                    res.code = 405;
                    res.set_header("Content-Type", "application/json");
                    crow::json::wvalue error_response;
                    error_response["error"] = "Method not allowed";
                    res.body = error_response.dump();
                    return res;
                }
            });

        // PUT /api/mappings/profiles/:profileName - Update an existing profile
        // DELETE /api/mappings/profiles/:profileName - Delete a profile
        CROW_ROUTE(app, "/api/mappings/profiles/<string>")
            .methods("PUT"_method, "DELETE"_method)
            ([this](const crow::request &req, const std::string &profileName) {
                if (req.method == crow::HTTPMethod::Put)
                {
                    return updateProfile(req, profileName);
                }
                else if (req.method == crow::HTTPMethod::Delete)
                {
                    return deleteProfile(req, profileName);
                }
                else
                {
                    crow::response res;
                    res.code = 405;
                    res.set_header("Content-Type", "application/json");
                    crow::json::wvalue error_response;
                    error_response["error"] = "Method not allowed";
                    res.body = error_response.dump();
                    return res;
                }
            });

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

    crow::response MappingConfigController::getAvailableProfiles(const crow::request &req)
    {
        try
        {
            std::vector<crow::json::wvalue> profiles;

            // Scan the mappings directory for CSV files
            if (std::filesystem::exists(mappingsDir_) && std::filesystem::is_directory(mappingsDir_))
            {
                for (const auto& entry : std::filesystem::directory_iterator(mappingsDir_))
                {
                    if (entry.is_regular_file() && entry.path().extension() == ".csv")
                    {
                        crow::json::wvalue profile;
                        profile["filename"] = entry.path().filename().string();
                        profile["path"] = entry.path().string();

                        // Get file size
                        profile["size_bytes"] = static_cast<int>(std::filesystem::file_size(entry.path()));

                        // Get last modified time
                        auto ftime = std::filesystem::last_write_time(entry.path());
                        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                            ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
                        );
                        auto time_t_value = std::chrono::system_clock::to_time_t(sctp);
                        profile["last_modified"] = static_cast<int64_t>(time_t_value);

                        profiles.push_back(std::move(profile));
                    }
                }
            }

            crow::json::wvalue response;
            response["mappings_directory"] = mappingsDir_;
            response["count"] = static_cast<int>(profiles.size());
            response["profiles"] = std::move(profiles);

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
            error_response["error"] = "Failed to list available profiles";
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

    crow::response MappingConfigController::createProfile(const crow::request &req)
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

            // Validate required fields
            if (!json.has("name") || !json.has("mappings"))
            {
                crow::response res;
                res.code = 400;
                res.set_header("Content-Type", "application/json");

                crow::json::wvalue error_response;
                error_response["error"] = "Missing required fields";
                error_response["message"] = "Request must include 'name' and 'mappings' fields";
                res.body = error_response.dump();
                return res;
            }

            std::string profileName = json["name"].s();

            // Validate profile name (prevent path traversal and ensure .csv extension)
            if (profileName.find("..") != std::string::npos ||
                profileName.find('/') != std::string::npos ||
                profileName.find('\\') != std::string::npos)
            {
                crow::response res;
                res.code = 400;
                res.set_header("Content-Type", "application/json");

                crow::json::wvalue error_response;
                error_response["error"] = "Invalid profile name";
                error_response["message"] = "Profile name contains invalid characters";
                res.body = error_response.dump();
                return res;
            }

            // Ensure .csv extension
            if (profileName.length() < 4 || profileName.substr(profileName.length() - 4) != ".csv")
            {
                profileName += ".csv";
            }

            std::filesystem::path profilePath = std::filesystem::path(mappingsDir_) / profileName;

            // Check if profile already exists
            if (std::filesystem::exists(profilePath))
            {
                crow::response res;
                res.code = 409;
                res.set_header("Content-Type", "application/json");

                crow::json::wvalue error_response;
                error_response["error"] = "Profile already exists";
                error_response["message"] = "A profile with this name already exists. Use PUT to update it.";
                error_response["profile"] = profileName;
                res.body = error_response.dump();
                return res;
            }

            // Validate mappings array
            auto mappings = json["mappings"];
            if (mappings.size() == 0)
            {
                crow::response res;
                res.code = 400;
                res.set_header("Content-Type", "application/json");

                crow::json::wvalue error_response;
                error_response["error"] = "Empty mappings array";
                error_response["message"] = "At least one mapping entry is required";
                res.body = error_response.dump();
                return res;
            }

            // Write CSV file
            std::ofstream file(profilePath);
            if (!file.is_open())
            {
                crow::response res;
                res.code = 500;
                res.set_header("Content-Type", "application/json");

                crow::json::wvalue error_response;
                error_response["error"] = "Failed to create file";
                error_response["message"] = "Could not open file for writing";
                res.body = error_response.dump();
                return res;
            }

            // Write CSV header
            file << "EventName,FieldName,PDUType,PDUField,Enabled,RateHz\n";
            file << "\n"; // Blank line after header (matches default format)

            // Write mapping entries
            int validEntries = 0;
            for (size_t i = 0; i < mappings.size(); ++i)
            {
                auto& mapping = mappings[i];

                // Validate required fields for each mapping
                if (!mapping.has("eventName") || !mapping.has("fieldName") ||
                    !mapping.has("pduType") || !mapping.has("pduField"))
                {
                    continue; // Skip invalid entries
                }

                std::string eventName = mapping["eventName"].s();
                std::string fieldName = mapping["fieldName"].s();
                std::string pduType = mapping["pduType"].s();
                std::string pduField = mapping["pduField"].s();
                bool enabled = mapping.has("enabled") ? mapping["enabled"].b() : true;
                double rateHz = mapping.has("rateHz") ? mapping["rateHz"].d() : 5.0;

                file << eventName << ","
                     << fieldName << ","
                     << pduType << ","
                     << pduField << ","
                     << (enabled ? "true" : "false") << ","
                     << rateHz << "\n";

                validEntries++;
            }

            file.close();

            if (validEntries == 0)
            {
                // Clean up empty file
                std::filesystem::remove(profilePath);

                crow::response res;
                res.code = 400;
                res.set_header("Content-Type", "application/json");

                crow::json::wvalue error_response;
                error_response["error"] = "No valid mapping entries";
                error_response["message"] = "All mapping entries were invalid or missing required fields";
                res.body = error_response.dump();
                return res;
            }

            crow::json::wvalue success_response;
            success_response["status"] = "success";
            success_response["message"] = "Profile created successfully";
            success_response["profile"] = profileName;
            success_response["entries_written"] = validEntries;
            success_response["path"] = profilePath.string();

            crow::response res;
            res.code = 201;
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
            error_response["error"] = "Failed to create profile";
            error_response["details"] = e.what();
            res.body = error_response.dump();
            return res;
        }
    }

    crow::response MappingConfigController::updateProfile(const crow::request &req, const std::string &profileName)
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

            // Validate required fields
            if (!json.has("mappings"))
            {
                crow::response res;
                res.code = 400;
                res.set_header("Content-Type", "application/json");

                crow::json::wvalue error_response;
                error_response["error"] = "Missing required field";
                error_response["message"] = "Request must include 'mappings' field";
                res.body = error_response.dump();
                return res;
            }

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
                error_response["message"] = "Profile name contains invalid characters";
                res.body = error_response.dump();
                return res;
            }

            std::filesystem::path profilePath = std::filesystem::path(mappingsDir_) / profileName;

            // Check if profile exists
            if (!std::filesystem::exists(profilePath))
            {
                crow::response res;
                res.code = 404;
                res.set_header("Content-Type", "application/json");

                crow::json::wvalue error_response;
                error_response["error"] = "Profile not found";
                error_response["message"] = "Cannot update a profile that does not exist";
                error_response["profile"] = profileName;
                res.body = error_response.dump();
                return res;
            }

            // Prevent updating the default profile if explicitly protected
            // (You can remove this check if you want to allow updating default-mapping.csv)
            if (profileName == "default-mapping.csv")
            {
                crow::response res;
                res.code = 403;
                res.set_header("Content-Type", "application/json");

                crow::json::wvalue error_response;
                error_response["error"] = "Cannot update default profile";
                error_response["message"] = "The default profile is protected and cannot be modified";
                res.body = error_response.dump();
                return res;
            }

            // Open file for writing (overwrite existing content)
            std::ofstream outFile(profilePath.string());
            if (!outFile.is_open())
            {
                crow::response res;
                res.code = 500;
                res.set_header("Content-Type", "application/json");

                crow::json::wvalue error_response;
                error_response["error"] = "Failed to open profile file for writing";
                error_response["profile"] = profileName;
                res.body = error_response.dump();
                return res;
            }

            // Write CSV header
            outFile << "EventName,FieldName,PDUType,PDUField,Enabled,RateHz\n";

            // Parse and write mappings
            int validEntries = 0;
            auto mappings = json["mappings"];
            for (size_t i = 0; i < mappings.size(); ++i)
            {
                auto mapping = mappings[i];

                // Validate required fields
                if (!mapping.has("eventName") || !mapping.has("fieldName") ||
                    !mapping.has("pduType") || !mapping.has("pduField"))
                {
                    continue; // Skip invalid entries
                }

                std::string eventName = mapping["eventName"].s();
                std::string fieldName = mapping["fieldName"].s();
                std::string pduType = mapping["pduType"].s();
                std::string pduField = mapping["pduField"].s();
                bool enabled = mapping.has("enabled") ? mapping["enabled"].b() : true;
                double rateHz = mapping.has("rateHz") ? mapping["rateHz"].d() : 0.0;

                outFile << eventName << "," << fieldName << "," << pduType << ","
                        << pduField << "," << (enabled ? "true" : "false") << ","
                        << rateHz << "\n";
                validEntries++;
            }

            outFile.close();

            if (validEntries == 0)
            {
                // If no valid entries, delete the file we just created
                std::filesystem::remove(profilePath);

                crow::response res;
                res.code = 400;
                res.set_header("Content-Type", "application/json");

                crow::json::wvalue error_response;
                error_response["error"] = "No valid mapping entries";
                error_response["message"] = "All mapping entries were invalid or missing required fields";
                res.body = error_response.dump();
                return res;
            }

            crow::json::wvalue success_response;
            success_response["status"] = "success";
            success_response["message"] = "Profile updated successfully";
            success_response["profile"] = profileName;
            success_response["entries_written"] = validEntries;
            success_response["path"] = profilePath.string();

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
            error_response["error"] = "Failed to update profile";
            error_response["details"] = e.what();
            res.body = error_response.dump();
            return res;
        }
    }

    crow::response MappingConfigController::deleteProfile(const crow::request &req, const std::string &profileName)
    {
        try
        {
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
                error_response["message"] = "Profile name contains invalid characters";
                res.body = error_response.dump();
                return res;
            }

            // Prevent deleting the default profile
            if (profileName == "default-mapping.csv")
            {
                crow::response res;
                res.code = 403;
                res.set_header("Content-Type", "application/json");

                crow::json::wvalue error_response;
                error_response["error"] = "Cannot delete default profile";
                error_response["message"] = "The default profile is protected and cannot be deleted";
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

            // Delete the file
            std::filesystem::remove(profilePath);

            crow::json::wvalue success_response;
            success_response["status"] = "success";
            success_response["message"] = "Profile deleted successfully";
            success_response["profile"] = profileName;

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
            error_response["error"] = "Failed to delete profile";
            error_response["details"] = e.what();
            res.body = error_response.dump();
            return res;
        }
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
