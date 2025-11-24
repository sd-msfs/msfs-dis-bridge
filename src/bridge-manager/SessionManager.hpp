/**
 * @file SessionManager.hpp
 * @brief Manager for multiple SimConnect sessions
 */

#pragma once
#include "SimSession.hpp"
#include <vector>
#include <memory>

/**
 * @class SessionManager
 * @brief Manages multiple simultaneous SimConnect sessions
 *
 * This class provides lifecycle management for multiple SimSession instances,
 * allowing the bridge system to connect to multiple instances of Microsoft
 * Flight Simulator simultaneously (or to the same instance with different
 * configurations).
 *
 * The SessionManager handles:
 * - Creating and starting new SimConnect sessions
 * - Tracking active sessions
 * - Stopping individual sessions by identifier
 * - Graceful shutdown of all sessions
 *
 * Each managed session runs independently in its own thread, processing
 * telemetry and forwarding data to the DIS encoding pipeline.
 *
 * @par Use Cases
 * - Multi-player scenarios: Multiple MSFS instances on a network
 * - Distributed simulation: Different aircraft types with different mapping configs
 * - Testing: Multiple sessions with varying update rates or field mappings
 *
 * @par Thread Safety
 * This class is not inherently thread-safe. It is designed to be used from
 * a single management thread (typically the REST API thread). If concurrent
 * access is needed, external synchronization must be provided.
 *
 * @par Example Usage
 * @code
 * SessionManager manager;
 * manager.addSession(0, "Fighter-1");
 * manager.addSession(1, "Transport-1");
 *
 * // Later, when shutting down:
 * manager.stopAll();
 * @endcode
 *
 * @see SimSession
 */
class SessionManager {
public:
    /**
     * @brief Default constructor
     */
    SessionManager() = default;

    /**
     * @brief Destructor - ensures all sessions are stopped
     *
     * Calls stopAll() to gracefully shut down any active sessions before
     * the manager is destroyed.
     */
    ~SessionManager();

    /**
     * @brief Creates and starts a new SimConnect session
     *
     * Instantiates a new SimSession with the specified configuration and name,
     * then starts it immediately. The session will run in a background thread
     * and begin streaming telemetry from MSFS.
     *
     * @param cfgIndex Configuration index for mapping profile selection
     * @param name Human-readable name for this session (e.g., "MSFS-Instance-1")
     *
     * @return true if the session was created and started successfully, false on error
     *
     * @note The session is managed via unique_ptr and will be automatically
     *       cleaned up when the SessionManager is destroyed or stopAll() is called
     *
     * @par Example
     * @code
     * SessionManager manager;
     * if (!manager.addSession(0, "Fighter-Aircraft")) {
     *     std::cerr << "Failed to start session\n";
     * }
     * @endcode
     *
     * @see SimSession::start()
     */
    bool addSession(uint32_t cfgIndex, const std::string& name);

    /**
     * @brief Stops a specific session by IP address identifier
     *
     * Locates and stops the session associated with the specified IP address.
     * This is useful when managing sessions that correspond to specific network
     * endpoints or client connections.
     *
     * @param ip IP address string identifying the session to stop
     *
     * @return true if a matching session was found and stopped, false otherwise
     *
     * @note The stopped session is removed from the manager's internal list
     *
     * @warning This method's implementation depends on IP-to-session mapping
     *          logic that may not be fully implemented in the current version.
     *          Consider using stopAll() or session name-based lookup instead.
     */
    bool stopSessionByIp(const std::string& ip);

    /**
     * @brief Stops all active sessions and clears the session list
     *
     * Iterates through all managed sessions and calls stop() on each,
     * then clears the internal session list. This ensures graceful shutdown
     * of all SimConnect connections.
     *
     * @note This method blocks until all sessions have fully terminated
     * @note Called automatically by the destructor
     *
     * @par Example
     * @code
     * SessionManager manager;
     * manager.addSession(0, "Session-1");
     * manager.addSession(1, "Session-2");
     *
     * // Shutdown all at once
     * manager.stopAll();
     * @endcode
     *
     * @see SimSession::stop()
     */
    void stopAll();

private:
    std::vector<std::unique_ptr<SimSession>> sessions_;  ///< Collection of managed SimSession instances
};
