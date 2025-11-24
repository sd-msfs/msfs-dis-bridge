/**
 * @file Encode.h
 * @brief DIS PDU encoder for converting FlightData to binary packets
 */

#pragma once
#include <cstdint>      // std::uint8_t
#include <vector>
#include "FlightData.h"

// forward-declare to avoid heavy includes here
class MappingConfig;

/**
 * @class Encode
 * @brief Encodes FlightData structures into binary DIS PDU packets
 *
 * This class converts internal FlightData telemetry into DIS (Distributed
 * Interactive Simulation) Protocol Data Units for network transmission. It
 * uses a MappingConfig to determine which fields to include, update rates,
 * and field-level enable/disable settings.
 *
 * The encoding process involves:
 * - Converting FlightData to InternalEvent representation
 * - Filtering fields based on mapping configuration
 * - Applying update rate throttling (Hz-based)
 * - Serializing enabled fields into binary DIS PDU format
 * - Proper byte ordering and alignment for network transmission
 *
 * @par Thread Safety
 * This class is not thread-safe. Each thread should use its own Encode
 * instance, or external synchronization must be provided.
 *
 * @par Example Usage
 * @code
 * MappingConfig config;
 * config.loadProfileFromCSV("mappings/high-frequency.csv");
 * Encode encoder(config);
 *
 * FlightData telemetry = getLatestTelemetry();
 * std::vector<uint8_t> pdu = encoder.encodeEvent(telemetry);
 * sendToNetwork(pdu);
 * @endcode
 *
 * @see Decode, FlightData, MappingConfig, InternalEvent
 */
class Encode {
public:
    /**
     * @brief Constructs an encoder with a specific mapping configuration
     *
     * @param cfg Reference to the mapping configuration that controls which
     *            fields are encoded, their update rates, and PDU structure.
     *            The config object must remain valid for the lifetime of this
     *            Encode instance.
     *
     * @note The MappingConfig reference is stored, not copied. Ensure the config
     *       object outlives this encoder.
     */
    explicit Encode(MappingConfig& cfg);

    /**
     * @brief Encodes FlightData into a binary DIS PDU
     *
     * Converts the provided flight telemetry data into a DIS Protocol Data Unit
     * suitable for network transmission. The method:
     * - Transforms FlightData into an InternalEvent
     * - Applies field-level filtering based on enabled mappings
     * - Enforces update rate limits (fields sent only at configured Hz)
     * - Serializes to binary DIS format
     *
     * @param fd FlightData structure containing current aircraft telemetry
     *
     * @return Binary buffer containing the complete DIS PDU, ready for UDP transmission
     *
     * @note If rate limiting prevents any fields from being sent, an empty vector
     *       may be returned. Check return value size before transmission.
     *
     * @par Performance
     * Encoding typically takes < 100 microseconds for standard Entity State PDUs.
     * The MappingConfig's rate limiting logic minimizes redundant network traffic.
     *
     * @par Field Filtering
     * Only fields marked as enabled in the MappingConfig will be included.
     * Additionally, fields are throttled according to their configured update
     * rates (Hz), so high-frequency calls to this method won't flood the network.
     */
    std::vector<std::uint8_t> encodeEvent(const FlightData& fd);

private:
    MappingConfig& config_;  ///< Reference to field mapping and rate control configuration
};