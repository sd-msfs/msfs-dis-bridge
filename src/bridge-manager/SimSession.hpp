/**
 * @file SimSession.hpp
 * @brief Individual simulation session management for SimConnect integration
 */
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <SimConnect.h>
#include "SimSession.hpp"
#include <iostream>

#include <windows.h>
#include <SimConnect.h>
#include <thread>
#include <atomic>
#include <string>
#include <cstdint>

/**
 * @brief Structure containing flight simulation data from MSFS
 *
 * This packed structure represents all the flight data variables that
 * are requested from Microsoft Flight Simulator via SimConnect.
 * The data corresponds to various aircraft parameters that are
 * transmitted over the DIS network.
 *
 * @note The structure uses pragma pack to ensure consistent memory
 * layout across different compilers and platforms
 */
#pragma pack(push, 1)
struct SimSample {
    /**
     * @brief Aircraft latitude in degrees
     *
     * Corresponds to MSFS SimVar "PLANE LATITUDE"
     */
    double lat;

    /**
     * @brief Aircraft longitude in degrees
     *
     * Corresponds to MSFS SimVar "PLANE LONGITUDE"
     */
    double lon;

    /**
     * @brief Aircraft altitude in meters
     *
     * Corresponds to MSFS SimVar "PLANE ALTITUDE"
     * @note Values are in meters, not feet
     */
    double alt;

    /**
     * @brief Aircraft pitch angle in degrees
     *
     * Corresponds to MSFS SimVar "PLANE PITCH DEGREES"
     * Positive values indicate nose up attitude
     */
    double pitch;

    /**
     * @brief Aircraft bank angle in degrees
     *
     * Corresponds to MSFS SimVar "PLANE BANK DEGREES"
     * Positive values indicate right bank
     */
    double bank;

    /**
     * @brief Aircraft heading in degrees true
     *
     * Corresponds to MSFS SimVar "PLANE HEADING DEGREES TRUE"
     * Values range from 0-360 degrees, referenced to true north
     */
    double heading;

    /**
     * @brief Velocity component along aircraft body X-axis
     *
     * Corresponds to MSFS SimVar "VELOCITY BODY X"
     * Forward/backward velocity in aircraft coordinate system
     */
    double velX;

    /**
     * @brief Velocity component along aircraft body Y-axis
     *
     * Corresponds to MSFS SimVar "VELOCITY BODY Y"
     * Left/right velocity in aircraft coordinate system
     */
    double velY;

    /**
     * @brief Velocity component along aircraft body Z-axis
     *
     * Corresponds to MSFS SimVar "VELOCITY BODY Z"
     * Up/down velocity in aircraft coordinate system
     */
    double velZ;

    /**
     * @brief Indicated airspeed
     *
     * Corresponds to MSFS SimVar "AIRSPEED INDICATED"
     * Airspeed as displayed on aircraft instruments
     */
    double airspeed;

    /**
     * @brief Vertical speed (rate of climb/descent)
     *
     * Corresponds to MSFS SimVar "VERTICAL SPEED"
     * Positive values indicate climb, negative indicate descent
     */
    double vs;

    /**
     * @brief Primary engine RPM
     *
     * Corresponds to MSFS SimVar "GENERAL ENG RPM:1"
     * RPM of the first/primary engine
     */
    double engRpm;   // GENERAL ENG RPM:1
};
#pragma pack(pop)

/**
 * @brief Individual simulation session managing one SimConnect connection
 *
 * The SimSession class represents a single connection to a Microsoft Flight
 * Simulator instance via SimConnect. Each session runs independently in its
 * own thread and manages the full lifecycle of data collection from MSFS.
 *
 * @details This class handles:
 * - Establishing and maintaining SimConnect connection
 * - Defining and requesting flight data variables
 * - Processing incoming simulation data
 * - Managing simulation state (running/paused)
 * - Thread-safe operation with atomic state variables
 *
 * @note Each SimSession operates independently and can connect to different
 * MSFS instances or use different configuration profiles.
 */
class SimSession {
public:
    /**
     * @brief Construct a new simulation session
     *
     * Initializes a new SimSession with the specified configuration index
     * and descriptive name. The session is not started until start() is called.
     *
     * @param configIndex Index identifying which configuration profile to use
     * @param name Human-readable name for this session (used for logging)
     *
     * @note Construction does not establish the SimConnect connection
     */
    SimSession(uint32_t configIndex, const std::string& name);

    /**
     * @brief Destructor - ensures proper cleanup
     *
     * Automatically stops the session and cleans up all resources
     * including SimConnect handles and thread termination.
     */
    ~SimSession();

    /**
     * @brief Start the simulation session
     *
     * Establishes the SimConnect connection, defines data requests,
     * and starts the background thread for data processing.
     *
     * @return true if the session was successfully started
     * @return false if startup failed (connection issues, invalid config, etc.)
     *
     * @note This method must be called before the session will begin
     * collecting data from MSFS
     *
     * @see stop()
     */
    bool start();

    /**
     * @brief Stop the simulation session
     *
     * Gracefully shuts down the session by stopping the background thread,
     * closing the SimConnect connection, and cleaning up resources.
     *
     * @note This method is automatically called by the destructor but
     * can be called explicitly if needed
     *
     * @see start()
     */
    void stop();

    /**
     * @brief Check if the simulation is currently paused
     *
     * @return true if MSFS is in any paused state
     * @return false if the simulation is running normally
     *
     * @note Uses atomic operations for thread-safe access
     */
    bool isPaused() const { return pauseFlags_.load(std::memory_order_relaxed) != 0; }

    /**
     * @brief Get detailed pause state flags
     *
     * @return Bitfield indicating specific pause states active in MSFS
     *
     * @note The returned value corresponds to MSFS pause flags and can
     * be used to determine the specific type of pause state
     */
    uint32_t pauseFlags() const { return pauseFlags_.load(std::memory_order_relaxed); }

    /**
     * @brief Check if the simulation is actively running
     *
     * @return true if the simulation is running and not paused
     * @return false if the simulation is stopped or paused
     *
     * @note Uses atomic operations for thread-safe access
     */
    bool isSimRunning() const { return simRunning_.load(std::memory_order_relaxed) != 0; }

    /**
     * @brief Get the session name
     *
     * @return The human-readable name assigned to this session
     *
     * @note Useful for logging and debugging multiple sessions
     */
    std::string getName() const { return name_; }

private:
    /**
     * @brief Static callback function for SimConnect dispatch
     *
     * This function serves as the entry point for all SimConnect callbacks.
     * It redirects calls to the appropriate SimSession instance.
     *
     * @param pData Pointer to received SimConnect data
     * @param cbData Size of the received data
     * @param ctx Context pointer (contains SimSession instance)
     */
    static void CALLBACK dispatchThunk(SIMCONNECT_RECV* pData, DWORD cbData, void* ctx);

    /**
     * @brief Main thread loop for SimConnect processing
     *
     * Runs in a separate thread to handle SimConnect events and data.
     * Continues processing until the session is stopped.
     */
    void runLoop_();

    /**
     * @brief Define data structures for SimConnect
     *
     * Sets up the SimConnect data definitions that specify which
     * flight parameters to request from MSFS.
     */
    void defineData_();

    /**
     * @brief Request data streaming from MSFS
     *
     * Initiates periodic data requests for the defined flight parameters.
     */
    void requestStream_();

    /**
     * @brief Process received SimConnect data
     *
     * Handles incoming data from SimConnect callbacks, including flight
     * data updates and simulation state changes.
     *
     * @param pData Pointer to received SimConnect data
     * @param cbData Size of the received data
     */
    void onDispatch_(SIMCONNECT_RECV* pData, DWORD cbData);

    /**
     * @brief SimConnect definition and request identifiers
     */
    enum {
        DEF_ID = 1, ///< Data definition ID
        REQ_ID = 100 ///< Data request ID
    };

    /**
     * @brief SimConnect event identifiers
     */
    enum : DWORD {
        EVT_PAUSE_EX1 = 1, ///< Extended pause event
        EVT_SIM = 2 ///< Simulation state event
    };

    uint32_t cfgIndex_; ///< Configuration index for this session
    std::string name_; ///< Human-readable session name

    HANDLE evt_{nullptr}; ///< Windows event handle for SimConnect
    HANDLE hSim_{nullptr}; ///< SimConnect connection handle
    std::thread th_; ///< Background processing thread
    std::atomic<bool> running_{false}; ///< Session running state

    std::atomic<uint32_t> pauseFlags_{0}; ///< Current pause state flags
    std::atomic<uint32_t> simRunning_{0}; ///< Simulation running state
};
