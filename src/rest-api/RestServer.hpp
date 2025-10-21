#pragma once
#include <crow.h>
#include <functional>
#include <optional>
#include <string>

class SessionManager;

// returns a ConfigIndex for a caller IP, or nullopt if unmapped
using IpToIndexFn = std::function<std::optional<int>(const std::string&)>;

class RestServer {
public:
    RestServer(SessionManager& mgr,
               IpToIndexFn ipToIndex,
               std::string bindAddr = "0.0.0.0",
               int port = 8080);

    // blocking run
    void run();

private:
    SessionManager& mgr_;
    IpToIndexFn     ipToIndex_;
    std::string     bind_;
    int             port_;

    static std::string clientIpFrom(const crow::request& req);
};
