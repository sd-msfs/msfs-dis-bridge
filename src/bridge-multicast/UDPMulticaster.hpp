/**
 * @file UDPMulticater.hpp
 * @brief UDP multicast sender for PDU transmission
 */

#pragma once
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include "ThreadSafeQueue.hpp"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

/**
 * @brief Singleton UDP multicast sender for DIS network communication
 *
 * The UDPMulticaster class provides a singleton interface for sending
 * DIS Protocol Data Units (PDUs) over UDP multicast. It manages a
 * background sender thread and a thread-safe queue for efficient
 * asynchronous transmission of PDU data.
 *
 * @details This class implements:
 * - Singleton pattern for application-wide multicast management
 * - Asynchronous PDU transmission using a background thread
 * - Thread-safe enqueueing of PDU data
 * - Configurable multicast parameters (group, port, TTL, loopback)
 * - Proper socket management and cleanup
 *
 * @note This implementation is Windows-specific, using Winsock2 APIs.
 * The class handles Winsock initialization and cleanup internally.
 *
 * @warning As a singleton, this class should not be copied or moved.
 * Access should only be through getInstance().
 */
class UDPMulticaster {
public:
    /**
     * @brief Get the singleton instance of UDPMulticaster
     *
     * Returns the single instance of UDPMulticaster, creating it if
     * it doesn't exist. Thread-safe initialization is guaranteed.
     *
     * @return Reference to the singleton UDPMulticaster instance
     *
     * @note The returned reference is valid for the lifetime of the
     * application. Do not attempt to delete or manage the lifetime
     * of this object.
     */
    static UDPMulticaster& getInstance();

    /**
     * @brief Start UDP multicast transmission
     *
     * Initializes the UDP multicast socket with the specified parameters
     * and starts the background sender thread. The multicast socket will
     * be configured according to the provided parameters.
     *
     * @param group Multicast group address (e.g., "239.1.2.3")
     * @param port UDP port number for multicast transmission
     * @param ifaddr Local interface IP address to bind to (empty string for any interface)
     * @param ttl Time-to-Live value for multicast packets (default: 1 - local network only)
     * @param loopback Whether to enable multicast loopback (default: false)
     *
     * @return true if multicast was successfully initialized and started
     * @return false if initialization failed (invalid parameters, socket errors, etc.)
     *
     * @note If already running, this method will stop the current session
     * before starting with new parameters.
     *
     * @warning TTL values:
     * - 1: Local network only
     * - 2-32: Local site/organization
     * - 33-64: Local region
     * - 65-128: Local continent
     * - 129-255: Unrestricted (global)
     *
     * @see stop(), enqueue()
     */
    bool start(const std::string& group, uint16_t port,
               const std::string& ifaddr = "", int ttl = 1, bool loopback = false);

    /**
     * @brief Stop UDP multicast transmission
     *
     * Gracefully shuts down the multicast sender by stopping the background
     * thread, closing the socket, and cleaning up resources. Any remaining
     * queued PDUs will be discarded.
     *
     * @note This method is safe to call multiple times and will have no
     * effect if the multicaster is not currently running.
     *
     * @warning Queued PDUs that haven't been transmitted will be lost
     * when stopping the multicaster.
     *
     * @see start(), enqueue()
     */
    void stop();

    /**
     * @brief Enqueue a PDU for multicast transmission
     *
     * Adds a Protocol Data Unit to the transmission queue. The PDU will
     * be sent asynchronously by the background sender thread. This method
     * is thread-safe and non-blocking.
     *
     * @param pdu Vector containing the complete PDU bytes to transmit
     *
     * @note The PDU data is moved into the queue for efficiency. After
     * calling this method, the passed vector should be considered empty.
     *
     * @warning This method will fail silently if the multicaster is not
     * currently running. Ensure start() has been called successfully.
     *
     * @see start(), stop()
     */
    void enqueue(std::vector<uint8_t> pdu);

private:
    /**
     * @brief Private constructor for singleton pattern
     *
     * Initializes the UDPMulticaster instance. Winsock initialization
     * is handled internally as needed.
     */
    UDPMulticaster();

    /**
     * @brief Private destructor for singleton pattern
     *
     * Ensures proper cleanup of resources including stopping any
     * active transmission and cleaning up Winsock resources.
     */
    ~UDPMulticaster();

    /**
     * @brief Background thread function for sending queued PDUs
     *
     * Runs continuously while the multicaster is active, dequeuing
     * PDU data and transmitting it over the multicast socket.
     * Handles transmission errors gracefully.
     */
    void senderLoop();

    /**
     * @brief Atomic flag indicating if the multicaster is running
     *
     * Used for thread-safe coordination between the main thread
     * and the background sender thread.
     */

     /**
      * @brief Background sender thread
      *
      * Thread that runs senderLoop() for asynchronous PDU transmission.
      * Joined during stop() to ensure clean shutdown.
      */
     std::atomic<bool> running_{false};

     /**
      * @brief UDP multicast socket handle
      *
      * Windows socket handle for the multicast UDP socket.
      * Set to INVALID_SOCKET when not active.
      */
     SOCKET sock_{INVALID_SOCKET};

     /**
      * @brief Background sender thread
      *
      * Thread that runs senderLoop() for asynchronous PDU transmission.
      * Joined during stop() to ensure clean shutdown.
      */
     std::thread sender_;

     /**
      * @brief Thread-safe queue for pending PDU transmissions
      *
      * Stores PDU data waiting to be transmitted by the background thread.
      * Provides thread-safe communication between enqueueing and sending.
      */
     ThreadSafeQueue<std::vector<uint8_t>> queue_;

     /**
      * @brief Current multicast group address
      *
      * String representation of the multicast IP address being used
      * for transmission (e.g., "239.1.2.3").
      */
     std::string group_;

     /**
      * @brief Current multicast port number
      *
      * UDP port number being used for multicast transmission.
      */
     uint16_t port_;

     /**
      * @brief Destination socket address structure
      *
      * Precomputed sockaddr_in structure containing the multicast
      * group and port information for efficient packet transmission.
      */
     sockaddr_in dst_; // Added declaration for destination address
};
