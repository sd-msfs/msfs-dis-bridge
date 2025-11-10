/**
 * @file SessionManager.hpp
 * @brief Session management for DIS Bridge connections
 */

#pragma once
#include "SimSession.hpp"
#include <vector>
#include <memory>

/**
 * @brief Manages multiple simulation sessions and their lifecycle
 *
 * The SessionManager class provides centralized management of multiple
 * simulation sessions within the MSFS DIS Bridge system. It handles
 * session creation, termination, and cleanup operations while maintaining
 * a collection of active sessions.
 *
 * @details This class serves as the primary interface for session management,
 * allowing the bridge system to maintain multiple concurrent connections
 * or simulation instances. Each session is managed through a SimSession
 * object that handles the specific details of that connection.
 *
 * @note The SessionManager uses RAII principles to ensure proper cleanup
 * of all managed sessions when the manager is destroyed.
 */
class SessionManager {
public:
    /**
     * @brief Default constructor
     *
     * Initializes an empty session manager with no active sessions.
     * Sessions must be explicitly added using addSession().
     */
    SessionManager() = default;

    /**
     * @brief Destructor - ensures all sessions are properly cleaned up
     *
     * Automatically stops and destroys all active sessions when the
     * SessionManager is destroyed, ensuring no resources are leaked.
     *
     * @note This will forcefully terminate all active sessions
     */
    ~SessionManager();

    /**
     * @brief Add a new simulation session to the manager
     *
     * Creates and starts a new simulation session with the specified
     * configuration and identifier. The session will be managed by
     * this SessionManager instance until explicitly stopped or the
     * manager is destroyed.
     *
     * @param cfgIndex Configuration index identifying which configuration
     *                 profile to use for this session
     * @param name Human-readable name for this session (for logging/debugging)
     * @return true if the session was successfully created and started
     * @return false if session creation failed (invalid config, resource issues, etc.)
     *
     * @note The session is automatically added to the internal collection
     * and will be managed by this SessionManager
     *
     * @see stopSessionByIp(), stopAll()
     */
    bool addSession(uint32_t cfgIndex, const std::string& name);

    /**
     * @brief Stop a specific session by its IP address
     *
     * Locates and terminates the session associated with the specified
     * IP address. The session is gracefully shut down and removed from
     * the active sessions collection.
     *
     * @param ip IP address string identifying the session to stop
     * @return true if a session with the specified IP was found and stopped
     * @return false if no session with that IP address was found
     *
     * @note The IP address should match the format used when the session
     * was created (e.g., "192.168.1.100")
     *
     * @see addSession(), stopAll()
     */
    bool stopSessionByIp(const std::string& ip);

    /**
     * @brief Stop all active sessions
     *
     * Gracefully terminates all currently active sessions managed by
     * this SessionManager. All sessions are properly shut down and
     * removed from the collection.
     *
     * @note This method is automatically called by the destructor, but
     * can be called explicitly if needed before destruction
     *
     * @see addSession(), stopSessionByIp()
     */
    void stopAll();

private:
    /**
     * @brief Collection of active simulation sessions
     *
     * Maintains unique ownership of all active SimSession objects.
     * Each session represents an active connection or simulation instance
     * being managed by this SessionManager.
     *
     * @note Uses unique_ptr for automatic memory management and clear
     * ownership semantics
     */
    std::vector<std::unique_ptr<SimSession>> sessions_;
};
