/**
 * @file UDPMulticaster.hpp
 * @brief Singleton UDP multicast sender for DIS PDU distribution
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
 * @class UDPMulticaster
 * @brief Singleton class for transmitting DIS PDUs via UDP multicast
 *
 * This class provides centralized management of UDP multicast transmission for
 * the bridge system. It receives encoded DIS Protocol Data Units from the
 * encoding pipeline and transmits them to a multicast group for network distribution.
 *
 * Key features:
 * - Singleton pattern for single multicast sender instance
 * - Background thread for non-blocking packet transmission
 * - Thread-safe PDU queueing via ThreadSafeQueue
 * - Configurable multicast group, port, TTL, and loopback
 * - Interface-specific binding support
 * - Winsock2 integration for Windows networking
 *
 * The multicaster operates asynchronously: PDUs are enqueued from SimSession threads
 * and transmitted by a dedicated sender thread, ensuring the encoding pipeline
 * never blocks on network I/O.
 *
 * @par Design Pattern
 * Uses the Meyers Singleton pattern for thread-safe lazy initialization.
 *
 * @par Threading Model
 * - Main thread: start(), stop(), enqueue() calls
 * - Sender thread: Background loop that dequeues and transmits PDUs
 * - SimSession threads: Call enqueue() with encoded PDUs
 *
 * @par Network Configuration
 * Supports standard DIS multicast addressing:
 * - Multicast group: Typically 239.x.x.x or 224.x.x.x
 * - Port: Configurable (DIS standard is 3000)
 * - TTL: Time-to-live for multicast packets (1 = local subnet)
 * - Loopback: Whether to receive own transmissions
 *
 * @par Thread Safety
 * All public methods are thread-safe. Multiple threads can safely call enqueue()
 * concurrently.
 *
 * @par Example Usage
 * @code
 * auto& multicaster = UDPMulticaster::getInstance();
 *
 * // Start multicasting to standard DIS multicast group
 * if (multicaster.start("239.1.2.3", 3000, "", 16, false)) {
 *     // Enqueue PDUs for transmission
 *     std::vector<uint8_t> pdu = encoder.encodeEvent(telemetry);
 *     multicaster.enqueue(std::move(pdu));
 *
 *     // Later, shutdown
 *     multicaster.stop();
 * }
 * @endcode
 *
 * @see ThreadSafeQueue, Encode, SimSession
 */
class UDPMulticaster {
public:
    /**
     * @brief Retrieves the singleton instance
     *
     * Thread-safe access to the global UDPMulticaster instance.
     * The instance is created on first call and persists for the program lifetime.
     *
     * @return Reference to the singleton UDPMulticaster
     *
     * @par Thread Safety
     * C++11 guarantees thread-safe initialization of function-local statics.
     */
    static UDPMulticaster& getInstance();

    /**
     * @brief Starts the multicast sender with specified network parameters
     *
     * Initializes the UDP multicast socket, configures multicast options,
     * and starts the background sender thread. The sender will continuously
     * dequeue and transmit PDUs until stop() is called.
     *
     * @param group Multicast group IP address (e.g., "239.1.2.3")
     * @param port UDP port number for multicast (e.g., 3000 for DIS)
     * @param ifaddr Optional interface address to bind to (empty = default interface)
     * @param ttl Time-to-live for multicast packets (1-255, default=1 for local subnet)
     * @param loopback Whether to receive own multicast packets (default=false)
     *
     * @return true if multicaster started successfully, false on error
     *
     * @par Common TTL Values
     * - 1: Local subnet only (default)
     * - 16: Site-wide (typical for campus/organization)
     * - 64: Region-wide
     * - 255: Global (unrestricted)
     *
     * @par Failure Cases
     * - Winsock initialization failure
     * - Invalid multicast group address
     * - Socket creation failure
     * - Socket option configuration failure
     * - Already running (call stop() first)
     *
     * @note This method blocks briefly while initializing the socket and starting
     *       the sender thread, but returns quickly.
     *
     * @par Example
     * @code
     * auto& mc = UDPMulticaster::getInstance();
     * // Local subnet multicast on standard DIS port
     * if (!mc.start("239.1.2.3", 3000)) {
     *     std::cerr << "Failed to start multicaster\n";
     * }
     * @endcode
     */
    bool start(const std::string& group, uint16_t port,
               const std::string& ifaddr = "", int ttl = 1, bool loopback = false);

    /**
     * @brief Stops the multicast sender and terminates the sender thread
     *
     * Gracefully shuts down the multicaster:
     * 1. Sets running flag to false
     * 2. Enqueues a dummy packet to unblock the sender thread
     * 3. Waits for sender thread to complete
     * 4. Closes the multicast socket
     * 5. Cleans up Winsock resources
     *
     * @note This method blocks until the sender thread has fully terminated
     * @note Safe to call multiple times; subsequent calls are no-ops
     * @note Any remaining queued PDUs will be discarded
     */
    void stop();

    /**
     * @brief Enqueues a DIS PDU for multicast transmission (thread-safe)
     *
     * Adds the provided PDU to the transmission queue. The sender thread will
     * dequeue and transmit it asynchronously. This method returns immediately
     * and never blocks on network I/O.
     *
     * @param pdu Binary DIS PDU data to transmit (will be moved)
     *
     * @note This method is thread-safe and can be called from multiple threads
     * @note The PDU is moved into the queue to avoid copying
     * @note Empty PDUs are accepted but will result in a zero-length transmission
     *
     * @par Thread Safety
     * Multiple SimSession threads can safely call enqueue() concurrently.
     *
     * @par Performance
     * This method is very fast (< 1 microsecond typical). The actual network
     * transmission happens asynchronously in the sender thread.
     *
     * @par Example
     * @code
     * auto& mc = UDPMulticaster::getInstance();
     * std::vector<uint8_t> pdu = encoder.encodeEvent(flightData);
     * mc.enqueue(std::move(pdu));  // Non-blocking
     * @endcode
     */
    void enqueue(std::vector<uint8_t> pdu);

private:
    /**
     * @brief Private constructor for singleton pattern
     *
     * Initializes Winsock on Windows platforms.
     */
    UDPMulticaster();

    /**
     * @brief Destructor - ensures clean shutdown
     *
     * Calls stop() to ensure the sender thread is terminated and resources
     * are released before destruction.
     */
    ~UDPMulticaster();

    /**
     * @brief Background thread loop for PDU transmission
     *
     * Continuously dequeues PDUs from the queue and transmits them via
     * the multicast socket. Runs until stop() sets the running flag to false.
     *
     * @note This method runs in a dedicated background thread
     * @note Blocks on queue.dequeue() when no PDUs are available
     */
    void senderLoop();

    std::atomic<bool> running_{false};                      ///< Flag indicating if sender thread is active
    SOCKET sock_{INVALID_SOCKET};                           ///< UDP multicast socket handle
    std::thread sender_;                                    ///< Background sender thread
    ThreadSafeQueue<std::vector<uint8_t>> queue_;           ///< Thread-safe PDU queue
    std::string group_;                                     ///< Multicast group IP address
    uint16_t port_;                                         ///< Multicast port number
    sockaddr_in dst_;                                       ///< Destination address structure for sendto()
};