/**
 * @file Encode.h
 * @brief Converts FlightData to DIS PDU format
 */

#pragma once
#include <cstdint>      // std::uint8_t
#include <vector>
#include "FlightData.h"

// forward-declare to avoid heavy includes here
class MappingConfig;

/**
 * @brief Encoder class that converts FlightData structures to DIS PDU format
 *
 * The Encode class provides functionality to transfrom flight simulation data
 * from MSFS 2024 into Distributed Interactive Simulation (DIS) Protocol Data Units (PDUs).
 * This enables DIS-compliant simulation systems to interface with MSFS to view
 * simulated flights that are happening in MSFS in real time.
 *
 * The encoding process involves:
 * 1. Converting FlightData to an internal event
 * 2. Mapping the event to a DIS PDU using configurable field mappings
 * 3. Marshaling the PDU into a network-ready byte format
 *
 * @see Decode
 * @see MappingConfig
 * @see FlightData
 */
class Encode {
public:

    /**
     * @brief Constructs an Encode object with specified mapping configuration
     *
     * Initializes the encoder with a reference to a MappingConfig that defines FlightData
     * fields will be mapped to DIS PDU fields. This configuration includes unit conversions,
     * coordinate system transformations, and field mappings for proper DIS compliance.
     *
     * @param cfg Reference to the MappingConfig used to map FlightData fields to the respective
     * PDU fields.
     *
     * @note The MappingConfig refrence must remain valid throughout the encoder's lifetime
     * @note Constructor is explicit to prevent accidental conversions
     * @see MappingConfig
     */
    explicit Encode(MappingConfig& cfg);

    /**
     * @brief Encodes FlightData into a DIS PDU byte array
     *
     * This method converts the FlightData structure into a formatted DIS PDU and returns
     * it as a byte vector for network transmission.
     *
     * @param fd The FlightData structure that contains aircraft state information
     * @return std::vector<std::uint8_t> Byte array containing the encoded DIS PDU
     * in network byte order ready for UDP multicast transmission
     *
     * @throws std::runtime_error if the mapping configuration fails to create a valid PDU
     * @throws std::runtime_error if PDU marshaling fails due to invalid data
     * @throws std::invalid_argument if the FlightData contains invalid or out-of-range values
     *
     * @note The returned byte array is in network byte order (big-endian) as required by DIS standard
     * @note Currently generates Entity State PDUs - other PDU types may be supported in future versions
     * @note The encoding process applies coordinate transformations as defined in the mapping configuration
     *
     * @see FlightData (input data structure)
     * @see MappingConfig::createEventFromFlightData()
     * @see MappingConfig::createPduFromEvent()
     *
     * @warning Ensure FlightData contains valid coordinate and orientation values before encoding
     */
    std::vector<std::uint8_t> encodeEvent(const FlightData& fd);

private:
    /** @brief Reference to the mapping configuration used for encoding operations */
    MappingConfig& config_;
};
