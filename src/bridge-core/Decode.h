/**
 * @file Decode.h
 * @brief Converts DIS PDU flight data to flight data that SimConnect recognizes
 */

#pragma once

#include <cstdint>   // std::uint8_t
#include <vector>
#include "FlightData.h"

class MappingConfig;

/**
 * @warning NOT CURRENTLY IMPLEMENTED - This class is for the future implementation of
 * a two way bridge
 *
 * @brief The Decode class allows the bridge to tranform incoming PDU packets
 * back into the FlightData structure.
 *
 * The Decode class provides the functionality to tranform incoming DIS packets
 * back into FlightData structures. This would transform the current one-way
 * bridge into a two-way one.
 *
 * It is the reverse operation of the Encode class.
 *
 * @todo Finish complete decoding pipeline
 * @todo Add support for different PDU types
 *
 * @see Encode
 * @see MappingConfig
 * @see FlightData
 *
 * Example usage:
 * @code{.cpp}
 * MappingConfig config;
 * Decode decoder(config);
 *
 * // Received PDU bytes from network
 * std::vector<uint8_t> pduBytes = receiveFromNetwork();
 *
 * try {
 *     FlightData flightData = decoder.decodePacket(pduBytes);
 *     // Use the decoded flight data...
 * } catch (const std::exception& e) {
 *     std::cerr << "Decoding failed: " << e.what() << std::endl;
 * }
 * @endcode
 */
class Decode {
public:
    /**
    * @brief Constructs a Decode object with the specified mapping configuration
    *
    * Initializes the decoder with a reference to a MappingConfig object that defines
    * how DIS PDU fields should be mapped back to FlightData structure fields. The
    * configuration must remain valid for the lifetime of the Decode object.
    *
    * @param config Reference to the MappingConfig object that defines field mappings
    *               and coordinate transformations for the decoding process
    *
    * @note The MappingConfig reference must remain valid throughout the decoder's lifetime
    * @see MappingConfig
    */
    Decode(MappingConfig& config);

    /**
     * @brief Decodes a DIS PDU into a FlightData structure
     *
     * Takes a byte buffer containing a PDU and converts it back into a FlightData struct
     *
     * @param buffer Byte vector containing the PDU data received from the network.
     * The buffer should contain a complete DIS PUD in network byte order.
     *
     * @return FlightData structure with decoded aircraft state information.
     *
     * @throws std::runtime_error if the PDU cannot be parsed from the buffer
     * @throws std::runtime_error if the mapping configuration fails to process the PDU
     * @throws std::invalid_argument if the buffer is empty or contains invalid data
     *
     * @note The input buffer is expected to be in network byte order (big-endian)
     * @note Only Entity State PDUs are currently supported for decoding
     *
     * @see FlightData
     * @see MappingConfig::createEventFromPdu()
     * @see MappingConfig::createFlightDataFromEvent()
     */
    FlightData decodePacket(const std::vector<uint8_t>& buffer);

private:
    /**
     * @brief Reference to the mapping configuration used for decoding
     *
     * Defines the reverse mapping from DIS PDU fields back to FlightData fields.
     * This includes coordinate transformation and unit conversions. The same congiguration
     * should typically be used for both encoding and decoding to ensure data consistency.
     */
    MappingConfig& config_;
};
