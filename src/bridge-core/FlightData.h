/**
 * @file FlightData.h
 * @brief Core flight simulation telemetry data structure
 */

#pragma once

/**
 * @struct FlightData
 * @brief Container for aircraft telemetry data from flight simulation
 *
 * This structure holds real-time aircraft state information received from
 * Microsoft Flight Simulator via SimConnect. The data is used for encoding
 * into DIS (Distributed Interactive Simulation) PDUs for network distribution.
 *
 * All position and orientation data follows standard aviation conventions:
 * - Latitude/Longitude: WGS84 geodetic coordinates
 * - Altitude: Above mean sea level (MSL)
 * - Angles: Euler angles in degrees
 * - Velocities: Body-frame coordinates in meters per second
 *
 * @note This is a plain-old-data (POD) structure with no member functions.
 * @see Encode, Decode, SimSession
 */
struct FlightData {
    double latitude;   ///< Aircraft latitude in degrees (-90 to +90, negative = South)
    double longitude;  ///< Aircraft longitude in degrees (-180 to +180, negative = West)
    double altitude;   ///< Aircraft altitude above MSL in meters
    double pitch;      ///< Pitch angle in degrees (nose up = positive)
    double bank;       ///< Bank/roll angle in degrees (right wing down = positive)
    double yaw;        ///< Yaw angle in degrees (clockwise from above = positive)
    double heading;    ///< Magnetic heading in degrees (0-360, 0 = North)
    double airspeed;   ///< Indicated airspeed in knots
    double velX;       ///< Velocity in body X-axis (forward) in m/s
    double velY;       ///< Velocity in body Y-axis (right) in m/s
    double velZ;       ///< Velocity in body Z-axis (down) in m/s
};
