/**
 * @file MappingConfigManager.h
 * @brief Thread-safe singleton manager for global MappingConfig access
 */

#pragma once

#include "MappingConfig.h"
#include <mutex>
#include <string>

/**
 * @class MappingConfigManager
 * @brief Thread-safe singleton manager for the global MappingConfig instance
 *
 * This class provides centralized, thread-safe access to the system's mapping
 * configuration. It ensures that the REST API, SimConnect sessions, and encoding/
 * decoding components all use the same consistent configuration.
 *
 * The manager implements the singleton pattern with mutex-protected operations,
 * making it safe for concurrent access from multiple threads. This is critical
 * because:
 * - REST API endpoints can reload configurations at runtime
 * - Multiple SimSession threads may be encoding telemetry simultaneously
 * - The UDPMulticaster runs in a background thread
 *
 * @par Design Pattern
 * This class uses the Meyers Singleton pattern (function-local static) which
 * provides:
 * - Thread-safe initialization (C++11 guarantee)
 * - Automatic cleanup on program exit
 * - Lazy initialization (created on first use)
 *
 * @par Thread Safety
 * All public methods are thread-safe. Internal mutex protection ensures that
 * configuration loads and accesses are properly synchronized.
 *
 * @par Example Usage
 * @code
 * // From any thread:
 * auto& manager = MappingConfigManager::getInstance();
 * manager.loadProfileFromCSV("mappings/high-frequency.csv");
 *
 * // Later, from another thread:
 * auto& config = MappingConfigManager::getInstance().getConfig();
 * auto event = config.createEventFromFlightData(telemetry);
 * @endcode
 *
 * @see MappingConfig, RestServer, SimSession
 */
class MappingConfigManager {
public:
    /**
     * @brief Retrieves the singleton instance
     *
     * Thread-safe access to the global MappingConfigManager instance.
     * The instance is created on first call and persists for the program lifetime.
     *
     * @return Reference to the singleton MappingConfigManager
     *
     * @par Thread Safety
     * C++11 guarantees thread-safe initialization of function-local statics.
     * Multiple concurrent calls will safely return the same instance.
     */
    static MappingConfigManager& getInstance();

    /**
     * @brief Retrieves the managed MappingConfig instance
     *
     * Provides access to the underlying configuration object for encoding,
     * decoding, and mapping operations.
     *
     * @return Reference to the global MappingConfig
     *
     * @warning The returned reference is valid for the lifetime of the manager,
     *          but the configuration content may change if loadProfileFromCSV()
     *          is called from another thread. Consider making a local copy if
     *          you need stable configuration for extended operations.
     */
    MappingConfig& getConfig();

    /**
     * @brief Loads a new mapping profile from a CSV file (thread-safe)
     *
     * Atomically replaces the current configuration with a new profile loaded
     * from the specified CSV file. The operation is protected by a mutex to
     * prevent race conditions with concurrent encoding/decoding operations.
     *
     * @param path Absolute or relative path to the CSV configuration file
     *
     * @return true if the profile was successfully loaded, false on error
     *
     * @par Thread Safety
     * This method is mutex-protected. If called while other threads are using
     * the configuration, it will block until safe to proceed. Once the new
     * profile is loaded, all subsequent operations will use the new settings.
     *
     * @par Example
     * @code
     * auto& manager = MappingConfigManager::getInstance();
     * if (manager.loadProfileFromCSV("mappings/low-bandwidth.csv")) {
     *     std::cout << "Switched to low-bandwidth profile\n";
     * }
     * @endcode
     *
     * @note The current profile name is updated on successful load
     * @see MappingConfig::loadProfileFromCSV
     */
    bool loadProfileFromCSV(const std::string& path);

    /**
     * @brief Retrieves the base directory for mapping CSV files
     *
     * @return Const reference to the mappings directory path
     *
     * @note This is typically set during application initialization
     */
    const std::string& getMappingsDir() const { return mappingsDir_; }

    /**
     * @brief Sets the base directory for mapping CSV files
     *
     * @param dir Absolute or relative path to the directory containing mapping profiles
     *
     * @note This should be called early in initialization before loading any profiles
     */
    void setMappingsDir(const std::string& dir) { mappingsDir_ = dir; }

    /**
     * @brief Retrieves the name of the currently loaded profile
     *
     * @return Const reference to the current profile filename (without path)
     *
     * @note Returns empty string if no profile has been loaded yet
     *
     * @par Example
     * @code
     * auto& manager = MappingConfigManager::getInstance();
     * std::cout << "Active profile: " << manager.getCurrentProfile() << "\n";
     * @endcode
     */
    const std::string& getCurrentProfile() const { return currentProfile_; }

private:
    /**
     * @brief Private constructor for singleton pattern
     *
     * Initializes the manager with default settings. Called only once by getInstance().
     */
    MappingConfigManager();

    /**
     * @brief Default destructor
     *
     * Cleans up resources when program exits.
     */
    ~MappingConfigManager() = default;

    // Delete copy/move constructors and assignment operators to enforce singleton
    MappingConfigManager(const MappingConfigManager&) = delete;             ///< No copy constructor
    MappingConfigManager& operator=(const MappingConfigManager&) = delete;  ///< No copy assignment
    MappingConfigManager(MappingConfigManager&&) = delete;                  ///< No move constructor
    MappingConfigManager& operator=(MappingConfigManager&&) = delete;       ///< No move assignment

    MappingConfig config_;          ///< The managed configuration instance
    std::mutex configMutex_;        ///< Mutex protecting configuration access and modification
    std::string mappingsDir_;       ///< Base directory for mapping CSV files
    std::string currentProfile_;    ///< Filename of currently loaded profile (without path)
};
