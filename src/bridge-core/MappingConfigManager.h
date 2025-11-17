#pragma once

#include "MappingConfig.h"
#include <mutex>
#include <string>

/**
 * Singleton manager for the global MappingConfig instance.
 * This allows the REST API and SimSession to share the same configuration.
 */
class MappingConfigManager {
public:
    // Get the singleton instance
    static MappingConfigManager& getInstance();

    // Get the mapping configuration
    MappingConfig& getConfig();

    // Load a profile from a CSV file (thread-safe)
    bool loadProfileFromCSV(const std::string& path);

    // Get the mappings directory
    const std::string& getMappingsDir() const { return mappingsDir_; }

    // Set the mappings directory
    void setMappingsDir(const std::string& dir) { mappingsDir_ = dir; }

    // Get the currently loaded profile name (if any)
    const std::string& getCurrentProfile() const { return currentProfile_; }

private:
    MappingConfigManager();
    ~MappingConfigManager() = default;

    // Delete copy/move constructors and assignment operators
    MappingConfigManager(const MappingConfigManager&) = delete;
    MappingConfigManager& operator=(const MappingConfigManager&) = delete;
    MappingConfigManager(MappingConfigManager&&) = delete;
    MappingConfigManager& operator=(MappingConfigManager&&) = delete;

    MappingConfig config_;
    std::mutex configMutex_;
    std::string mappingsDir_;
    std::string currentProfile_;
};
