/**
 * @file FlightData.h
 * @brief Flight data structure for aircraft state information
*/

#pragma once

/**
 * @brief Structure containing aircraft flight state data from
 * Microsoft Flight Simulator 2024
 *
 * This strucuture holds all of the flight data extracted from Microsoft
 * Flight Simulator using the Simconnect API. This structure is used to store
 * flight data for the various process inbetween extraction and encoding.
 *
 * The structure currently contains all data necessary for filling the Entity
 * State PDU from DIS.
 *
 * @note Adding additional variables will require strict ordering as the
 * strucuture is filled from an enumerable returned from SimConnect. Be sure
 * to request data from SimConnect as ordered in this structure.
 *
 * The following code is the FlightData structure in its entirity
 * @code{.cpp}
 * struct FlightData {
 *     double latitude;
 *     double longitude;
 *     double altitude;
 *     double pitch;
 *     double bank;
 *     double yaw;
 *     double heading;
 *     double airspeed;
 *     double velX;
 *     double velY;
 *     double velZ;
 * };
 * @endcode
 *
 * @note The velocity vectors are currently unused but can be used for future
 * implementation of interpolation via dead reckoning algorithms
 */
struct FlightData {
    /**
     * @brief Aircraft latitude
     */
    double latitude;

    /**
     * @brief Aircraft longitude
     */
    double longitude;

    /**
     * @brief Aircraft altitude in feet
     */
    double altitude;

    /**
     * @brief Aircraft pitch angle in radians
     */
    double pitch;

    /**
     * @brief Aircraft bank (roll) angle in radians
     */
    double bank;

    /**
     * @brief Aircraft yaw angle in radians
     */
    double yaw;

    /**
     * @brief Aircraft yaw angle in radians
     */
    double heading;

    /**
     * @brief Aircraft indicated airspeed in meters per second
     */
    double airspeed;

    /**
     * @brief Velocity X-axis component in m/s
     */
    double velX;

    /**
     * @brief Velocity Y-axis component in m/s
     */
    double velY;

    /**
     * @brief Velocity Z-axis component in m/s
     */
    double velZ;
};
