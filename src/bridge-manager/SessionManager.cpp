#include "SessionManager.hpp"
#include <iostream>

SessionManager::~SessionManager() {
    stopAll();
}

bool SessionManager::addSession(uint32_t cfgIndex, const std::string& name) {
    auto s = std::make_unique<SimSession>(cfgIndex, name);
    if (!s->start()) {
        std::cerr << "Failed to start " << name << "\n";
        return false;
    }
    sessions_.push_back(std::move(s));
    return true;
}

bool SessionManager::stopSessionByIp(const std::string& ip) {
    // For simplicity, assume session name is "MSFS_<ip>"
    std::string targetName = "MSFS_" + ip;
    for (auto it = sessions_.begin(); it != sessions_.end(); ++it) {
        if ((*it)->getName() == targetName) {
            (*it)->stop();
            sessions_.erase(it);
            return true;
        }
    }
    return false;
}

void SessionManager::stopAll() {
    for (auto& s : sessions_) {
        if (s) s->stop();
    }
    sessions_.clear();
}
