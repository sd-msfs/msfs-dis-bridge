/**
 * @file SimSession.hpp
 * @brief SimConnect integration for Microsoft Flight Simulator telemetry streaming
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
 * @struct SimSample
 * @brief Aircraft telemetry data structure received from Microsoft Flight Simulator
 *
 * This structure defines the exact data layout for SimConnect data subscriptions.
 * Each field corresponds to a specific SimConnect simulation variable that is
 * continuously streamed from MSFS to the bridge application.
 *
 * @note The #pragma pack(push, 1) directive ensures tight packing with no padding
 *       bytes, which is critical for correct binary data interpretation from SimConnect.
 *
 * @par SimConnect Variables Mapped
 * - PLANE LATITUDE (degrees)
 * - PLANE LONGITUDE (degrees)
 * - PLANE ALTITUDE (meters above MSL)
 * - PLANE PITCH DEGREES (Euler angle)
 * - PLANE BANK DEGREES (Euler angle)
 * - PLANE HEADING DEGREES TRUE (true heading, not magnetic)
 * - VELOCITY BODY X/Y/Z (body-frame velocities in m/s)
 * - AIRSPEED INDICATED (knots)
 * - VERTICAL SPEED (feet per minute or m/s depending on units)
 * - GENERAL ENG RPM:1 (engine 1 RPM)
 *
 * @see SimSession::defineData_(), SimSession::onDispatch_()
 */
#pragma pack(push, 1)
struct SimSample {
    double lat;      ///< Aircraft latitude in degrees (negative = South)
    double lon;      ///< Aircraft longitude in degrees (negative = West)
    double alt;      ///< Aircraft altitude above MSL in meters
    double pitch;    ///< Pitch angle in degrees (nose up = positive)
    double bank;     ///< Bank/roll angle in degrees (right wing down = positive)
    double heading;  ///< True heading in degrees (0-360, 0 = North)
    double velX;     ///< Velocity in body X-axis (forward) in m/s
    double velY;     ///< Velocity in body Y-axis (right) in m/s
    double velZ;     ///< Velocity in body Z-axis (down) in m/s
    double airspeed; ///< Indicated airspeed in knots
    double vs;       ///< Vertical speed (positive = climbing)
    double engRpm;   ///< Engine 1 RPM
};
#pragma pack(pop)

/**
 * @class SimSession
 * @brief Manages an independent SimConnect connection to Microsoft Flight Simulator
 *
 * Each SimSession instance establishes a dedicated connection to a running instance
 * of Microsoft Flight Simulator 2024 (or earlier versions) via the SimConnect API.
 * It continuously streams aircraft telemetry data and forwards it to the DIS encoding
 * pipeline for network distribution.
 *
 * Key responsibilities:
 * - Establishing and maintaining SimConnect connection
 * - Defining data subscriptions for aircraft telemetry
 * - Receiving continuous data updates from MSFS
 * - Monitoring simulation pause state and running status
 * - Running in a dedicated background thread for non-blocking operation
 * - Converting SimSample data to FlightData and encoding to DIS PDUs
 *
 * @par Threading Model
 * Each SimSession runs in its own background thread that:
 * 1. Establishes the SimConnect connection
 * 2. Defines data subscriptions
 * 3. Enters a message dispatch loop (runLoop_)
 * 4. Processes SimConnect callbacks via dispatchThunk
 * 5. Forwards telemetry to encoding and UDP multicast
 *
 * @par Thread Safety
 * - Public methods (start, stop, getters) are thread-safe via atomics
 * - Internal state is managed within the dedicated thread
 * - Callbacks are dispatched on the SimConnect thread
 *
 * @par Connection Lifecycle
 * @code
 * SimSession session(0, "MSFS-Instance-1");
 * if (session.start()) {
 *     // Session is now streaming telemetry in background thread
 *     while (session.isSimRunning() && !session.isPaused()) {
 *         // Monitor status
 *     }
 *     session.stop();  // Graceful shutdown
 * }
 * @endcode
 *
 * @see SimSample, SessionManager, FlightData, Encode
 */
class SimSession {
public:
    /**
     * @brief Constructs a new SimSession for a specific configuration
     *
     * @param configIndex Index of the configuration profile to use (for mapping configs)
     * @param name Human-readable name for this session (e.g., "MSFS-Instance-1")
     *
     * @note The session is not started automatically; call start() to begin streaming
     */
    SimSession(uint32_t configIndex, const std::string& name);

    /**
     * @brief Destructor - ensures clean shutdown of SimConnect connection
     *
     * Automatically calls stop() if the session is still running.
     */
    ~SimSession();

    /**
     * @brief Starts the SimConnect session and begins telemetry streaming
     *
     * Creates a background thread that:
     * 1. Establishes SimConnect connection to MSFS
     * 2. Defines data subscriptions (SimSample structure)
     * 3. Subscribes to system events (pause, sim start/stop)
     * 4. Enters message dispatch loop for continuous data reception
     *
     * @return true if session started successfully, false on error
     *
     * @note This method returns immediately after starting the background thread.
     *       The actual connection establishment happens asynchronously.
     *
     * @par Failure Cases
     * - MSFS is not running
     * - SimConnect.dll is not available
     * - Previous session is still running
     * - Thread creation failed
     */
    bool start();

    /**
     * @brief Stops the SimConnect session and terminates telemetry streaming
     *
     * Gracefully shuts down the SimConnect connection:
     * 1. Sets running flag to false
     * 2. Signals the dispatch loop to exit
     * 3. Waits for background thread to complete
     * 4. Closes SimConnect handle
     *
     * @note This method blocks until the background thread has fully terminated
     * @note Safe to call multiple times; subsequent calls are no-ops
     */
    void stop();

    /**
     * @brief Checks if the simulation is currently paused
     *
     * @return true if MSFS is in a paused state (any pause flag set), false otherwise
     *
     * @note Thread-safe via atomic memory operations
     * @see pauseFlags()
     */
    bool isPaused() const { return pauseFlags_.load(std::memory_order_relaxed) != 0; }

    /**
     * @brief Retrieves the current pause flags from MSFS
     *
     * @return Bitfield of pause flags from SimConnect PAUSE_EX1 event
     *
     * @note Pause flags indicate various pause modes (ESC menu, Active Pause, etc.)
     * @note Thread-safe via atomic memory operations
     */
    uint32_t pauseFlags() const { return pauseFlags_.load(std::memory_order_relaxed); }

    /**
     * @brief Checks if the simulation is currently running
     *
     * @return true if MSFS simulation is active (not on menu, not loading), false otherwise
     *
     * @note Thread-safe via atomic memory operations
     * @note This is distinct from pause state; simulation can be running but paused
     */
    bool isSimRunning() const { return simRunning_.load(std::memory_order_relaxed) != 0; }

    /**
     * @brief Retrieves the human-readable name of this session
     *
     * @return Session name string (e.g., "MSFS-Instance-1")
     */
    std::string getName() const { return name_; }

private:
    /**
     * @brief Static callback function for SimConnect message dispatch
     *
     * This is the entry point for all SimConnect callbacks. It forwards the call
     * to the appropriate SimSession instance via the context pointer.
     *
     * @param pData Pointer to received SimConnect message data
     * @param cbData Size of the received data in bytes
     * @param ctx Context pointer (points to the SimSession instance)
     *
     * @note This must be a static CALLBACK function per SimConnect API requirements
     * @see onDispatch_()
     */
    static void CALLBACK dispatchThunk(SIMCONNECT_RECV* pData, DWORD cbData, void* ctx);

    /**
     * @brief Main message dispatch loop running in background thread
     *
     * Continuously processes SimConnect messages until stop() is called.
     * Handles connection management, data reception, and event processing.
     */
    void runLoop_();

    /**
     * @brief Defines SimConnect data subscriptions
     *
     * Registers the SimSample structure layout with SimConnect, specifying
     * which simulation variables to stream.
     */
    void defineData_();

    /**
     * @brief Initiates continuous data streaming from MSFS
     *
     * Requests SimConnect to begin sending SimSample data at regular intervals.
     */
    void requestStream_();

    /**
     * @brief Instance-specific dispatch handler for SimConnect messages
     *
     * Processes received SimConnect messages including:
     * - SIMCONNECT_RECV_SIMOBJECT_DATA (telemetry updates)
     * - SIMCONNECT_RECV_EVENT_EX1 (pause state changes)
     * - SIMCONNECT_RECV_EVENT (sim start/stop events)
     *
     * @param pData Pointer to received message
     * @param cbData Size of message in bytes
     */
    void onDispatch_(SIMCONNECT_RECV* pData, DWORD cbData);

    enum { DEF_ID = 1, REQ_ID = 100 };  ///< SimConnect definition and request identifiers
    enum : DWORD { EVT_PAUSE_EX1 = 1, EVT_SIM = 2 };  ///< Event subscription identifiers

    uint32_t cfgIndex_;     ///< Configuration index for mapping profile
    std::string name_;      ///< Human-readable session name

    HANDLE evt_{nullptr};   ///< Event handle for SimConnect message signaling
    HANDLE hSim_{nullptr};  ///< SimConnect connection handle
    std::thread th_;        ///< Background thread for message dispatch loop
    std::atomic<bool> running_{false};  ///< Flag indicating if session is active

    std::atomic<uint32_t> pauseFlags_{0};   ///< Current MSFS pause state flags
    std::atomic<uint32_t> simRunning_{0};   ///< Current MSFS simulation running state
};
