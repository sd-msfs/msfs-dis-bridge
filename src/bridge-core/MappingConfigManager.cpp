#include "MappingConfigManager.h"
#include <filesystem>
#include <iostream>

MappingConfigManager& MappingConfigManager::getInstance() {
    static MappingConfigManager instance;
    return instance;
}

MappingConfigManager::MappingConfigManager()
    : mappingsDir_("mappings"), currentProfile_("") {
}

MappingConfig& MappingConfigManager::getConfig() {
    return config_;
}

bool MappingConfigManager::loadProfileFromCSV(const std::string& path) {
    std::lock_guard<std::mutex> lock(configMutex_);

    if (!config_.loadProfileFromCSV(path)) {
        std::cerr << "[MappingConfigManager] Failed to load profile from: " << path << std::endl;
        return false;
    }

    // Store the profile name (just the filename, not the full path)
    std::filesystem::path p(path);
    currentProfile_ = p.filename().string();

    std::cout << "[MappingConfigManager] Loaded profile: " << currentProfile_
              << " (" << config_.getMappings().size() << " mappings)" << std::endl;

    return true;
}
