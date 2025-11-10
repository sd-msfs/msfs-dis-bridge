/**
 * @file mappingConfig.h
 * @brief Configuration system for mapping between MSFS data and DIS PDUs
 */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <memory>
#include "FlightData.h"

/**
 * @namespace DIS
 * @brief Distributed Interactive Simulation protocol namespace
 */
namespace DIS {
    /**
     * @brief Forward declaration for DIS Protocol Data Unit
     */
    class Pdu;

    /**
     * @brief Forward declaration for DIS data stream
     */
    class DataStream;
}

/**
 * @brief Internal event structure for inter-module communication
 *
 * This structure represents events that flow between different components
 * of the MSFS DIS Bridge system, carrying typed data with associated parameters.
 */
struct InternalEvent {
    /**
     * @brief Type identifier for the event
     *
     * Describes the nature of the event (e.g., "FlightDataUpdate", "SystemChange")
     */
    std::string eventType;

    /**
     * @brief Event parameters as key-value pairs
     *
     * Contains the actual data associated with the event, where keys are
     * parameter names and values are numeric data.
     */
    std::unordered_map<std::string, double> parameters;
};

/**
 * @brief Configuration entry defining mapping between MSFS data and DIS PDUs
 *
 * Each MappingEntry defines how a specific piece of flight simulation data
 * should be mapped to/from DIS Protocol Data Units, including rate limiting
 * and enabling/disabling capabilities.
 */
struct MappingEntry {
    /**
     * @brief Name of the internal event this mapping handles
     *
     * This should correspond to event names used throughout the system
     * for consistent data flow.
     */
    std::string eventName;

    /**
     * @brief Specific field name within the event data
     *
     * Identifies which parameter within the event's data this mapping
     * is responsible for handling.
     */
    std::string fieldName;

    /**
     * @brief Type of DIS PDU this mapping targets
     *
     * Specifies which DIS PDU type (e.g., "EntityStatePdu", "FirePdu")
     * this mapping will generate or consume.
     *
     * @note Only the Entity State PDU is currently implemented
     */
    std::string pduType;

    /**
     * @brief Specific field within the target PDU
     *
     * Identifies which field in the DIS PDU structure this mapping
     * will read from or write to.
     */
    std::string pduField;

    /**
     * @brief Whether this mapping is currently active
     *
     * Allows individual mappings to be enabled or disabled without
     * removing them from the configuration.
     */
    bool enabled;

    /**
     * @brief Update rate in Hz for this mapping
     *
     * Defines how frequently this mapping should be processed.
     * - 0 = event-driven only (no periodic updates)
     * - >0 = maximum updates per second
     *
     * @note Used for rate limiting to prevent excessive network traffic
     */
    double rateHz; // 0 = event-driven only

    /**
     * @brief Timestamp of last transmission
     *
     * Used internally for rate limiting calculations. Tracks when this
     * mapping was last processed to enforce the specified rate limits.
     */
    std::chrono::steady_clock::time_point lastSent; // for rate limiting
};

/**
 * @brief Central configuration manager for MSFS-DIS data mapping
 *
 * The MappingConfig class manages the configuration that defines how
 * Microsoft Flight Simulator data is mapped to and from Distributed
 * Interactive Simulation (DIS) Protocol Data Units (PDUs).
 *
 * @details This class serves as the bridge between different data formats,
 * providing conversion methods and maintaining mapping configurations
 * loaded from external files. It supports both event-driven and
 * rate-limited data transmission.
 */
class MappingConfig {
public:
    /**
    * @brief Load mapping configuration from a CSV file
    *
    * Reads a CSV file containing mapping definitions and populates
    * the internal mapping table. The CSV should contain columns
    * corresponding to the MappingEntry structure fields.
    *
    * @param path File system path to the CSV configuration file
    * @return true if the file was successfully loaded and parsed
    * @return false if there was an error reading or parsing the file
    *
    * @note Any existing mappings are replaced when a new profile is loaded
    */
    bool loadProfileFromCSV(const std::string& path);

    /**
     * @brief Get read-only access to current mapping configuration
     *
     * @return const reference to the vector of mapping entries
     *
     * @note The returned reference is valid until the next call to
     * loadProfileFromCSV() or object destruction
     */
    const std::vector<MappingEntry>& getMappings() const { return mappings_; }

    /**
     * @brief Convert FlightData to internal event format
     *
     * Transforms flight simulation data into the internal event structure
     * used for inter-module communication within the bridge system.
     *
     * @param fd Flight data structure containing simulation state
     * @return InternalEvent containing the converted data
     *
     * @note Required by the encoding pipeline
     *
     */
    InternalEvent createEventFromFlightData(const FlightData& fd);

    /**
     * @brief Generate DIS PDU from internal event
     *
     * Creates the appropriate DIS Protocol Data Unit based on the
     * event data and current mapping configuration.
     *
     * @param event Internal event containing data to be transmitted
     * @return Smart pointer to the created PDU, or nullptr if no mapping exists
     *
     * @note Required by the encoding pipeline
     */
    std::unique_ptr<DIS::Pdu> createPduFromEvent(const InternalEvent& event);

    /**
     * @brief Convert received DIS PDU to internal event
     *
     * Parses an incoming DIS PDU and converts it to the internal event
     * format for processing by the bridge system.
     *
     * @param pdu The received DIS Protocol Data Unit
     * @return InternalEvent containing the extracted data
     *
     * @note Required by the decoding pipeline
     */
    InternalEvent createEventFromPdu(const DIS::Pdu& pdu);

    /**
     * @brief Convert internal event back to FlightData format
     *
     * Transforms internal event data back into the FlightData structure
     * for injection into the flight simulator.
     *
     * @param event Internal event containing simulation data
     * @return FlightData structure ready for simulator consumption
     *
     * @note Required by the decoding pipeline
     */
    FlightData createFlightDataFromEvent(const InternalEvent& event);

    /**
     * @brief Find mapping configuration by event name
     *
     * Helper method to locate a specific mapping entry based on
     * the event name it handles.
     *
     * @param eventName Name of the event to search for
     * @return Pointer to the matching MappingEntry, or nullptr if not found
     *
     * @note The returned pointer is valid until the mapping configuration
     * is reloaded or the object is destroyed
     */
    const MappingEntry* getMapping(const std::string& eventName) const;

private:
    /**
     * @brief Internal storage for mapping configuration entries
     *
     * Contains all currently loaded mapping definitions that control
     * the behavior of the MSFS-DIS bridge system.
     */
    std::vector<MappingEntry> mappings_;
};
