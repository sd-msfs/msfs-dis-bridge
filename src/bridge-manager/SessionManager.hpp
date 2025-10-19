#pragma once
#include "SimSession.hpp"
#include <vector>
#include <memory>

class SessionManager {
public:
    SessionManager() = default;
    ~SessionManager();

    bool addSession(uint32_t cfgIndex, const std::string& name);
    bool stopSessionByIp(const std::string& ip);
    void stopAll();

private:
    std::vector<std::unique_ptr<SimSession>> sessions_;
};
