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

void SessionManager::stopAll() {
    for (auto& s : sessions_) {
        if (s) s->stop();
    }
    sessions_.clear();
}
