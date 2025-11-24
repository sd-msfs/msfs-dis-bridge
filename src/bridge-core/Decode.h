/**
 * @file Decode.h
 * @brief DIS PDU decoder for converting binary packets to FlightData
 */

#pragma once

#include <cstdint>   // std::uint8_t
#include <vector>
#include "FlightData.h"

class MappingConfig;

/**
 * @class Decode
 * @brief Decodes incoming DIS PDU packets into FlightData structures
 *
 * This class provides functionality to deserialize binary DIS (Distributed
 * Interactive Simulation) Protocol Data Units (PDUs) into the internal
 * FlightData representation. It uses a MappingConfig to determine which
 * PDU fields map to which FlightData members.
 *
 * The decoder handles:
 * - Binary buffer parsing and validation
 * - PDU header interpretation
 * - Field extraction according to mapping configuration
 * - Data type conversion and unit normalization
 *
 * @par Thread Safety
 * This class is not thread-safe. Each thread should maintain its own Decode
 * instance, or external synchronization must be provided.
 *
 * @par Example Usage
 * @code
 * MappingConfig config;
 * config.loadProfileFromCSV("mappings/default.csv");
 * Decode decoder(config);
 *
 * std::vector<uint8_t> pduBuffer = receiveFromNetwork();
 * FlightData telemetry = decoder.decodePacket(pduBuffer);
 * @endcode
 *
 * @see Encode, FlightData, MappingConfig
 */
class Decode {
public:
    /**
     * @brief Constructs a decoder with a specific mapping configuration
     *
     * @param config Reference to the mapping configuration that defines how
     *               PDU fields correspond to FlightData members. The config
     *               object must remain valid for the lifetime of this Decode instance.
     *
     * @note The MappingConfig reference is stored, not copied. Ensure the config
     *       object outlives this decoder.
     */
    Decode(MappingConfig& config);

    /**
     * @brief Decodes a binary DIS PDU into a FlightData structure
     *
     * Parses the provided binary buffer as a DIS PDU and extracts flight
     * telemetry data according to the configured field mappings. The method
     * validates PDU structure and converts data to appropriate units.
     *
     * @param buffer Binary PDU data received from the network. Must contain
     *               a complete, well-formed DIS PDU.
     *
     * @return FlightData object populated with decoded telemetry values
     *
     * @throws std::runtime_error If buffer is malformed or contains invalid PDU
     * @throws std::out_of_range If buffer is too short for the expected PDU type
     *
     * @note Unmapped or disabled fields in the mapping config will have default
     *       values (typically 0.0) in the returned FlightData.
     *
     * @par Performance
     * This operation performs binary parsing and is optimized for low latency.
     * Typical decode time is < 100 microseconds for standard Entity State PDUs.
     */
    FlightData decodePacket(const std::vector<uint8_t>& buffer);

private:
    MappingConfig& config_;  ///< Reference to field mapping configuration
};