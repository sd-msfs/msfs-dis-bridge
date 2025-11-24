/**
 * @file MappingConfigController.h
 * @brief REST API controller for managing field mapping configurations
 */

#pragma once

#include <crow.h>
#include <string>
#include <mutex>

// Forward declarations
class MappingConfig;
struct MappingEntry;

namespace DISBridge::Controllers
{

    /**
     * @class MappingConfigController
     * @brief HTTP REST controller for mapping configuration management
     *
     * This controller exposes HTTP endpoints for runtime management of field mappings
     * between FlightData and DIS PDUs. It allows clients to:
     * - Query current mapping configurations
     * - List available mapping profiles (CSV files)
     * - Create new mapping profiles
     * - Update existing profiles
     * - Delete profiles
     * - Modify individual field mappings
     * - Reload configurations from disk
     *
     * All operations are thread-safe and affect the global MappingConfig instance
     * managed by MappingConfigManager.
     *
     * @par HTTP Endpoints
     * - GET /api/mappings - Retrieve current mapping configuration
     * - GET /api/mappings/profiles - List available mapping profiles
     * - GET /api/mappings/entry/{eventName} - Get specific mapping entry
     * - POST /api/mappings/profiles - Create new mapping profile
     * - PUT /api/mappings/profiles/{profileName} - Update existing profile
     * - DELETE /api/mappings/profiles/{profileName} - Delete a profile
     * - PATCH /api/mappings/entry - Update individual field mapping
     * - POST /api/mappings/reload - Reload configuration from disk
     *
     * @par Request/Response Format
     * All requests and responses use JSON. Example mapping entry:
     * @code
     * {
     *   "eventName": "EntityState_Lat",
     *   "fieldName": "latitude",
     *   "pduType": "EntityStatePDU",
     *   "pduField": "location.lat",
     *   "enabled": true,
     *   "rateHz": 20.0
     * }
     * @endcode
     *
     * @par Thread Safety
     * All methods are protected by an internal mutex to ensure thread-safe
     * access to the mapping configuration.
     *
     * @see MappingConfig, MappingConfigManager, MappingEntry
     */
    class MappingConfigController
    {
    public:
        /**
         * @brief Constructs a mapping configuration controller
         *
         * @param mappingConfig Reference to the MappingConfig instance to manage
         * @param mappingsDir Directory path where mapping CSV files are stored
         *
         * @note The MappingConfig reference must remain valid for the lifetime
         *       of this controller
         */
        MappingConfigController(MappingConfig& mappingConfig, const std::string& mappingsDir);

        /**
         * @brief Registers all mapping-related routes with the Crow application
         *
         * This method sets up all HTTP endpoint handlers for mapping management.
         * It should be called during server initialization before starting the
         * Crow app.
         *
         * @param app Reference to the Crow SimpleApp instance
         *
         * @par Registered Routes
         * - GET /api/mappings
         * - GET /api/mappings/profiles
         * - GET /api/mappings/entry/<eventName>
         * - POST /api/mappings/profiles
         * - PUT /api/mappings/profiles/<profileName>
         * - DELETE /api/mappings/profiles/<profileName>
         * - PATCH /api/mappings/entry
         * - POST /api/mappings/reload
         */
        void registerRoutes(crow::SimpleApp &app);

        /**
         * @brief Retrieves the current mapping configuration (GET /api/mappings)
         *
         * Returns a JSON array of all currently active mapping entries, including
         * field names, PDU mappings, enabled status, and update rates.
         *
         * @param req HTTP request object
         * @return HTTP response with JSON array of mapping entries
         *
         * @par Response Format
         * @code
         * {
         *   "mappings": [
         *     {
         *       "eventName": "EntityState_Lat",
         *       "fieldName": "latitude",
         *       "pduType": "EntityStatePDU",
         *       "pduField": "location.lat",
         *       "enabled": true,
         *       "rateHz": 20.0
         *     },
         *     ...
         *   ]
         * }
         * @endcode
         */
        crow::response getCurrentMappings(const crow::request &req);

        /**
         * @brief Lists all available mapping profiles (GET /api/mappings/profiles)
         *
         * Scans the mappings directory for CSV files and returns their names.
         *
         * @param req HTTP request object
         * @return HTTP response with JSON array of profile names
         *
         * @par Response Format
         * @code
         * {
         *   "profiles": ["default.csv", "high-frequency.csv", "low-bandwidth.csv"]
         * }
         * @endcode
         */
        crow::response getAvailableProfiles(const crow::request &req);

        /**
         * @brief Creates a new mapping profile (POST /api/mappings/profiles)
         *
         * Accepts a JSON payload containing mapping entries and creates a new
         * CSV file in the mappings directory.
         *
         * @param req HTTP request with JSON payload containing profile data
         * @return HTTP response indicating success or validation errors
         *
         * @par Request Body
         * @code
         * {
         *   "profileName": "custom-profile.csv",
         *   "mappings": [...]
         * }
         * @endcode
         */
        crow::response createProfile(const crow::request &req);

        /**
         * @brief Updates an existing profile (PUT /api/mappings/profiles/{name})
         *
         * Replaces the contents of an existing mapping profile with new data.
         *
         * @param req HTTP request with JSON payload
         * @param profileName Name of the profile to update
         * @return HTTP response indicating success or errors
         */
        crow::response updateProfile(const crow::request &req, const std::string &profileName);

        /**
         * @brief Deletes a mapping profile (DELETE /api/mappings/profiles/{name})
         *
         * Removes a CSV mapping profile from the mappings directory.
         *
         * @param req HTTP request object
         * @param profileName Name of the profile to delete
         * @return HTTP response indicating success or errors
         *
         * @warning This permanently deletes the file from disk
         */
        crow::response deleteProfile(const crow::request &req, const std::string &profileName);

        /**
         * @brief Updates a single mapping entry (PATCH /api/mappings/entry)
         *
         * Modifies the properties of an individual field mapping (e.g., enable/disable,
         * change update rate) without reloading the entire configuration.
         *
         * @param req HTTP request with JSON payload containing updated entry
         * @return HTTP response indicating success or validation errors
         *
         * @par Request Body
         * @code
         * {
         *   "eventName": "EntityState_Alt",
         *   "enabled": false,
         *   "rateHz": 10.0
         * }
         * @endcode
         */
        crow::response updateMapping(const crow::request &req);

        /**
         * @brief Retrieves a specific mapping entry (GET /api/mappings/entry/{name})
         *
         * Returns detailed information about a single mapping entry by event name.
         *
         * @param req HTTP request object
         * @param eventName Name of the mapping entry to retrieve
         * @return HTTP response with mapping entry details or 404 if not found
         */
        crow::response getMappingEntry(const crow::request &req, const std::string &eventName);

        /**
         * @brief Reloads the current mapping profile from disk (POST /api/mappings/reload)
         *
         * Re-reads the active mapping profile CSV file, applying any external changes
         * made to the file. Useful for development and dynamic reconfiguration.
         *
         * @param req HTTP request object
         * @return HTTP response indicating success or errors
         *
         * @note This operation affects all active SimConnect sessions immediately
         */
        crow::response reloadMappings(const crow::request &req);

    private:
        MappingConfig& mappingConfig_;    ///< Reference to the managed mapping configuration
        std::string mappingsDir_;         ///< Directory containing mapping CSV files
        mutable std::mutex config_mutex_; ///< Mutex protecting configuration access

        /**
         * @brief Validates a mapping entry JSON object
         *
         * Checks that all required fields are present and have valid values.
         *
         * @param entry JSON object to validate
         * @param errorMsg Output parameter for error description
         * @return true if valid, false otherwise
         */
        bool validateMappingEntry(const crow::json::rvalue& entry, std::string& errorMsg) const;

        /**
         * @brief Converts a MappingEntry to JSON format
         *
         * @param entry Mapping entry to format
         * @return JSON representation of the entry
         */
        crow::json::wvalue formatMappingEntry(const MappingEntry& entry) const;
    };

}
