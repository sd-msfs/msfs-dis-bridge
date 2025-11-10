/**
 * @file RestServer.hpp
 * @brief REST API server for DIS Bridge management
 */

#pragma once
#include <crow.h>
#include <functional>
#include <optional>
#include <string>

/**
 * @brief Forward declaration of SessionManager
 */
class SessionManager;

/**
 * @brief Function type for mapping client IP addresses to configuration indices
 *
 * This function type defines the signature for callbacks that resolve
 * client IP addresses to configuration profile indices. Used to determine
 * which configuration a particular client should use.
 *
 * @param clientIp The IP address string of the connecting client
 * @return Optional configuration index, or std::nullopt if no mapping exists
 *
 * @note The configuration index should correspond to a valid configuration
 * profile that can be used with SessionManager::addSession()
 */
using IpToIndexFn = std::function<std::optional<int>(const std::string&)>;

/**
 * @brief REST API server for managing MSFS DIS Bridge sessions
 *
 * The RestServer class provides a web-based REST API interface for managing
 * simulation sessions in the MSFS DIS Bridge system. It allows remote clients
 * to start, stop, and monitor bridge sessions through HTTP requests.
 *
 * @details The server provides endpoints for:
 * - Session management (start/stop operations)
 * - Status monitoring and health checks
 * - Configuration and diagnostic information
 * - Client IP-based configuration mapping
 *
 * The server uses the Crow web framework for HTTP handling and integrates
 * with the SessionManager for actual session operations. Client requests
 * can be mapped to specific configurations based on their IP addresses.
 *
 * @note This server is designed to run in a blocking mode and should
 * typically be executed in its own thread when integrated into a larger
 * application.
 */
class RestServer {
public:

    /**
     * @brief Construct a new REST server instance
     *
     * Creates a REST server configured to manage the provided SessionManager
     * using the specified IP-to-configuration mapping function.
     *
     * @param mgr Reference to the SessionManager that will handle session operations
     * @param ipToIndex Function to map client IP addresses to configuration indices
     * @param bindAddr IP address to bind the server to (default: "0.0.0.0" - all interfaces)
     * @param port TCP port number to listen on (default: 8080)
     *
     * @note The SessionManager reference must remain valid for the lifetime
     * of the RestServer instance.
     *
     * @note Common bind addresses:
     * - "0.0.0.0": Listen on all available network interfaces
     * - "127.0.0.1": Listen only on localhost (local connections only)
     * - Specific IP: Listen only on that specific interface
     *
     * @see run()
     */
    RestServer(SessionManager& mgr,
               IpToIndexFn ipToIndex,
               std::string bindAddr = "0.0.0.0",
               int port = 8080);

    /**
     * @brief Start the REST server (blocking operation)
     *
     * Starts the HTTP server and begins listening for client requests.
     * This method blocks the calling thread until the server is stopped
     * (typically via signal or external shutdown mechanism).
     *
     * The server will handle incoming HTTP requests and route them to
     * appropriate handlers for session management operations.
     *
     * @note This method blocks indefinitely. In multi-threaded applications,
     * consider running this in a dedicated thread.
     *
     * @warning The server will continue running until explicitly stopped
     * or the process is terminated. Ensure proper shutdown mechanisms
     * are in place.
     */
    void run();

private:
    /**
     * @brief Reference to the session manager
     *
     * The SessionManager instance that handles the actual session
     * creation, termination, and management operations requested
     * through the REST API.
     */
    SessionManager& mgr_;

    /**
     * @brief IP-to-configuration mapping function
     *
     * Function used to determine which configuration index should
     * be used for requests from specific client IP addresses.
     * Returns std::nullopt for unmapped IPs.
     */
    IpToIndexFn     ipToIndex_;

    /**
     * @brief Server bind address
     *
     * IP address that the server will bind to for incoming connections.
     * Can be a specific IP address or "0.0.0.0" for all interfaces.
     */
    std::string     bind_;

    /**
     * @brief Server listening port
     *
     * TCP port number that the server will listen on for HTTP requests.
     */
    int             port_;

    /**
     * @brief Extract client IP address from HTTP request
     *
     * Utility method to extract the client's IP address from an incoming
     * HTTP request. Handles various scenarios including proxy headers
     * and direct connections.
     *
     * @param req The incoming Crow HTTP request object
     * @return String representation of the client's IP address
     *
     * @note This method considers proxy headers (X-Forwarded-For, etc.)
     * to determine the real client IP when behind reverse proxies.
     */
    static std::string clientIpFrom(const crow::request& req);
};
