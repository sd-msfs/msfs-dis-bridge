/**
 * @file InternalEvent.h
 * @brief Generic event structure for inter-module communication
 */

#pragma once

#include <string>
#include <unordered_map>

/**
 * @struct InternalEvent
 * @brief Generic event container for passing data between system modules
 *
 * This structure provides a flexible mechanism for inter-module communication
 * within the bridge system. It uses a name-value pair approach where the event
 * name identifies the type of data, and the payload contains key-value mappings
 * of numeric data fields.
 *
 * The InternalEvent serves as an intermediate representation between FlightData
 * and DIS PDUs, allowing the MappingConfig system to selectively map only
 * enabled fields according to the active configuration profile.
 *
 * @par Thread Safety
 * This structure is copyable and movable, but individual instances are not
 * inherently thread-safe. Synchronization must be handled by the caller.
 *
 * @par Example Usage
 * @code
 * InternalEvent evt;
 * evt.name = "EntityStatePDU";
 * evt.payload["latitude"] = 28.5;
 * evt.payload["longitude"] = -81.3;
 * evt.payload["altitude"] = 1000.0;
 * @endcode
 *
 * @see FlightData, MappingConfig, Encode
 */
struct InternalEvent {
    /**
     * @typedef Payload
     * @brief Type alias for the payload map structure
     *
     * Maps field names (strings) to numeric values (doubles). This allows
     * flexible field-level control over which telemetry data is included
     * in the encoded DIS PDU.
     */
    using Payload = std::unordered_map<std::string, double>;

    /**
     * @brief Name identifier for this event
     *
     * Typically corresponds to the PDU type or event category, such as
     * "EntityStatePDU", "FlightDataUpdate", or a custom mapping profile name.
     */
    std::string name;

    /**
     * @brief Key-value pairs of numeric telemetry data
     *
     * Each key represents a field name (e.g., "latitude", "pitch", "airspeed")
     * and the corresponding value is the numeric measurement in appropriate units.
     * The specific fields included depend on the active MappingConfig profile.
     */
    Payload payload;
};
