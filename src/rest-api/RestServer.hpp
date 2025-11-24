/**
 * @file RestServer.hpp
 * @brief HTTP REST API server for bridge management and monitoring
 */

#pragma once
#include <crow.h>
#include <functional>
#include <optional>
#include <string>

class SessionManager;

/**
 * @typedef IpToIndexFn
 * @brief Function type for mapping client IP addresses to configuration indices
 *
 * This function determines which mapping configuration profile should be used
 * for a client based on their IP address. It enables multi-tenant scenarios
 * where different clients or aircraft may use different field mappings.
 *
 * @param clientIp IP address string of the requesting client
 * @return Optional configuration index (0-based), or std::nullopt if unmapped
 *
 * @par Example
 * @code
 * auto ipMapper = [](const std::string& ip) -> std::optional<int> {
 *     if (ip == "192.168.1.100") return 0;  // Fighter config
 *     if (ip == "192.168.1.101") return 1;  // Transport config
 *     return std::nullopt;  // No mapping
 * };
 * @endcode
 */
using IpToIndexFn = std::function<std::optional<int>(const std::string&)>;

/**
 * @class RestServer
 * @brief HTTP REST API server for managing the DIS bridge system
 *
 * This class provides a Crow-based HTTP server that exposes REST endpoints for:
 * - SimConnect session management (start/stop sessions)
 * - Mapping configuration control (load/update profiles, modify field settings)
 * - System metrics and monitoring (network stats, performance, resource usage)
 * - Health checks and status queries
 *
 * The server operates in a blocking mode on a dedicated thread and handles
 * concurrent HTTP requests from multiple clients. It integrates with the
 * SessionManager for SimConnect lifecycle management and MappingConfigManager
 * for runtime configuration updates.
 *
 * @par HTTP Endpoints
 * The server registers various controllers that provide endpoints such as:
 * - GET /health - System health status
 * - GET /metrics - Performance and resource metrics
 * - GET /mappings - Current field mapping configuration
 * - POST /mappings/reload - Reload mapping profile from CSV
 * - POST /sessions/start - Start new SimConnect session
 * - POST /sessions/stop - Stop active session
 *
 * See individual controller documentation for complete endpoint listings.
 *
 * @par IP-Based Configuration Routing
 * The server uses an IpToIndexFn callback to map client IPs to configuration
 * indices, enabling multi-client scenarios with different mapping profiles.
 *
 * @par Thread Model
 * The run() method blocks the calling thread and processes HTTP requests
 * in Crow's internal thread pool. It should be called from a dedicated
 * REST API thread.
 *
 * @par Example Usage
 * @code
 * SessionManager sessionMgr;
 * auto ipMapper = [](const std::string& ip) -> std::optional<int> {
 *     return 0;  // Default config for all clients
 * };
 *
 * RestServer server(sessionMgr, ipMapper, "0.0.0.0", 8080);
 * server.run();  // Blocks until server is stopped
 * @endcode
 *
 * @see SessionManager, MappingConfigManager, MappingConfigController, MetricsController
 */
class RestServer {
public:
    /**
     * @brief Constructs a REST API server with specified configuration
     *
     * @param mgr Reference to the SessionManager for SimConnect lifecycle control
     * @param ipToIndex Callback function mapping client IPs to configuration indices
     * @param bindAddr IP address to bind the server to (default: "0.0.0.0" = all interfaces)
     * @param port TCP port number for HTTP server (default: 8080)
     *
     * @note The server does not start automatically; call run() to begin serving requests
     * @note The SessionManager reference must remain valid for the lifetime of this server
     *
     * @par Common Bind Addresses
     * - "0.0.0.0" - Listen on all network interfaces (default)
     * - "127.0.0.1" - Localhost only (development/testing)
     * - Specific IP - Bind to single network interface
     */
    RestServer(SessionManager& mgr,
               IpToIndexFn ipToIndex,
               std::string bindAddr = "0.0.0.0",
               int port = 8080);

    /**
     * @brief Starts the HTTP server and blocks until shutdown (blocking call)
     *
     * Initializes the Crow application, registers all HTTP endpoints via
     * controllers, and begins serving HTTP requests on the configured address
     * and port. This method blocks the calling thread indefinitely until the
     * server is externally stopped (e.g., via signal or shutdown endpoint).
     *
     * @note This method BLOCKS the calling thread. Run it in a dedicated thread
     *       if you need concurrent operations in the main thread.
     *
     * @par Lifecycle
     * 1. Crow app initialization
     * 2. Controller registration (MappingConfigController, MetricsController, etc.)
     * 3. Socket binding on configured address:port
     * 4. HTTP request processing loop (blocks here)
     * 5. Cleanup on shutdown
     *
     * @par Example
     * @code
     * RestServer server(sessionMgr, ipMapper);
     *
     * // Option 1: Blocking main thread
     * server.run();
     *
     * // Option 2: Dedicated thread
     * std::thread apiThread([&server]() { server.run(); });
     * apiThread.detach();
     * @endcode
     *
     * @throws std::runtime_error If unable to bind to specified address/port
     */
    void run();

private:
    SessionManager& mgr_;      ///< Reference to SessionManager for SimConnect control
    IpToIndexFn     ipToIndex_; ///< IP-to-config-index mapping function
    std::string     bind_;      ///< Bind address for HTTP server
    int             port_;      ///< TCP port number for HTTP server

    /**
     * @brief Extracts the client IP address from an HTTP request
     *
     * Parses the Crow request object to determine the originating client IP.
     * Checks X-Forwarded-For header first (for proxy scenarios), then falls
     * back to direct connection IP.
     *
     * @param req Crow HTTP request object
     * @return Client IP address as string
     *
     * @note Handles reverse proxy scenarios via X-Forwarded-For header
     */
    static std::string clientIpFrom(const crow::request& req);
};
