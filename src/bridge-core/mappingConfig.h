/**
 * @file mappingConfig.h
 * @brief Configuration system for FlightData to DIS PDU field mappings
 */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <memory>
#include "FlightData.h"

// Forward declarations
namespace DIS {
    class Pdu;       ///< DIS Protocol Data Unit base class
    class DataStream; ///< DIS binary stream handler
}

/**
 * @struct InternalEvent
 * @brief Internal representation of telemetry events for mapping transformations
 *
 * This structure serves as an intermediate format between FlightData and DIS PDUs.
 * It allows flexible field-level mapping and filtering.
 */
struct InternalEvent {
    std::string eventType;  ///< Event type identifier (e.g., "EntityStatePDU")
    std::unordered_map<std::string, double> parameters;  ///< Named parameters with numeric values
};

/**
 * @struct MappingEntry
 * @brief Single field mapping configuration entry
 *
 * Defines how a specific FlightData field maps to a DIS PDU field, including
 * enable/disable state and rate limiting configuration.
 *
 * Each entry controls:
 * - Source field from FlightData (e.g., "latitude")
 * - Target PDU type and field (e.g., "EntityStatePDU.location.lat")
 * - Whether this field is currently enabled
 * - Update rate limit in Hz (0 = no rate limit, send every update)
 * - Timestamp tracking for rate enforcement
 */
struct MappingEntry {
    std::string eventName;  ///< Event/field identifier in FlightData
    std::string fieldName;  ///< FlightData field name (e.g., "latitude")
    std::string pduType;    ///< DIS PDU type (e.g., "EntityStatePDU")
    std::string pduField;   ///< Target field in PDU (e.g., "location.lat")
    bool enabled;           ///< Whether this field mapping is active
    double rateHz;          ///< Update rate limit in Hz (0 = event-driven only, no throttling)
    std::chrono::steady_clock::time_point lastSent;  ///< Timestamp of last transmission for rate limiting
};

/**
 * @class MappingConfig
 * @brief Manages field-level mappings between FlightData and DIS PDUs
 *
 * This class provides the core configuration system for translating between
 * internal FlightData structures and external DIS Protocol Data Units. It
 * supports:
 * - CSV-based configuration loading
 * - Per-field enable/disable control
 * - Rate limiting (Hz-based) for each field
 * - Bidirectional transformation (FlightData ↔ DIS PDU)
 *
 * The mapping system allows fine-grained control over which telemetry fields
 * are transmitted and at what frequencies, enabling bandwidth optimization
 * and selective data sharing.
 *
 * @par Configuration File Format (CSV)
 * The CSV file should contain columns:
 * - eventName: Identifier for this mapping
 * - fieldName: FlightData member name
 * - pduType: DIS PDU type identifier
 * - pduField: Target field in PDU structure
 * - enabled: "true" or "false"
 * - rateHz: Numeric update rate (Hz), 0 for no throttling
 *
 * @par Example Configuration
 * @code
 * eventName,fieldName,pduType,pduField,enabled,rateHz
 * EntityState_Lat,latitude,EntityStatePDU,location.lat,true,20.0
 * EntityState_Lon,longitude,EntityStatePDU,location.lon,true,20.0
 * EntityState_Alt,altitude,EntityStatePDU,location.alt,true,10.0
 * EntityState_Pitch,pitch,EntityStatePDU,orientation.pitch,true,30.0
 * @endcode
 *
 * @par Thread Safety
 * This class is not inherently thread-safe. Use MappingConfigManager for
 * thread-safe access via mutex protection.
 *
 * @see MappingConfigManager, FlightData, InternalEvent, Encode, Decode
 */
class MappingConfig {
public:
    /**
     * @brief Loads a mapping profile from a CSV file
     *
     * Parses the specified CSV file and populates the internal mappings table.
     * The file must follow the expected format with all required columns.
     *
     * @param path Absolute or relative path to the CSV configuration file
     *
     * @return true if the profile was successfully loaded, false on error
     *
     * @note Previous mappings are cleared before loading new profile
     * @note Invalid rows in the CSV will be logged but won't halt loading
     *
     * @par Example
     * @code
     * MappingConfig config;
     * if (!config.loadProfileFromCSV("mappings/high-frequency.csv")) {
     *     std::cerr << "Failed to load mapping profile\n";
     * }
     * @endcode
     */
    bool loadProfileFromCSV(const std::string& path);

    /**
     * @brief Retrieves all configured mapping entries
     *
     * @return Const reference to the vector of all mapping entries
     */
    const std::vector<MappingEntry>& getMappings() const { return mappings_; }

    /**
     * @brief Converts FlightData to an InternalEvent representation
     *
     * Transforms the provided FlightData into an intermediate InternalEvent
     * structure, applying field filtering based on enabled mappings and
     * rate limiting constraints.
     *
     * @param fd FlightData structure containing current telemetry
     *
     * @return InternalEvent containing only enabled, rate-limited fields
     *
     * @note Fields disabled in the configuration will be omitted
     * @note Fields subject to rate limits will only appear if sufficient
     *       time has elapsed since last transmission
     */
    InternalEvent createEventFromFlightData(const FlightData& fd);

    /**
     * @brief Creates a DIS PDU from an InternalEvent
     *
     * Serializes the provided InternalEvent into the appropriate DIS PDU
     * structure based on the event type and configured mappings.
     *
     * @param event InternalEvent containing field data to encode
     *
     * @return Unique pointer to the constructed DIS PDU object
     *
     * @note The returned PDU is ready for binary serialization and transmission
     */
    std::unique_ptr<DIS::Pdu> createPduFromEvent(const InternalEvent& event);

    /**
     * @brief Extracts an InternalEvent from a received DIS PDU
     *
     * Parses the provided DIS PDU and converts it to an InternalEvent
     * representation according to the configured field mappings.
     *
     * @param pdu Received DIS PDU to decode
     *
     * @return InternalEvent containing extracted field values
     *
     * @note Only fields present in the mapping configuration will be extracted
     */
    InternalEvent createEventFromPdu(const DIS::Pdu& pdu);

    /**
     * @brief Converts an InternalEvent back to FlightData
     *
     * Reconstructs a FlightData structure from an InternalEvent, reversing
     * the transformation performed by createEventFromFlightData().
     *
     * @param event InternalEvent containing field data
     *
     * @return FlightData structure populated with values from the event
     *
     * @note Fields not present in the event will have default values (0.0)
     */
    FlightData createFlightDataFromEvent(const InternalEvent& event);

    /**
     * @brief Finds a mapping entry by event name
     *
     * @param eventName Name of the event to search for
     *
     * @return Pointer to the MappingEntry if found, nullptr otherwise
     *
     * @note The returned pointer is valid only while the MappingConfig exists
     *       and no new profile is loaded
     */
    const MappingEntry* getMapping(const std::string& eventName) const;

private:
    std::vector<MappingEntry> mappings_;  ///< All configured field mappings
};