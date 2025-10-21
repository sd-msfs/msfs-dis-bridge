#include "RestServer.hpp"
#include "SessionManager.hpp"
#include <algorithm>
#include <cctype>

static std::string trim(std::string s) {
    auto notspace = [](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
    return s;
}

// Prefer X-Forwarded-For (if behind a proxy), else Crow's remote address.
std::string RestServer::clientIpFrom(const crow::request& req) {
    if (auto xff = req.get_header_value("X-Forwarded-For"); !xff.empty()) {
        auto comma = xff.find(',');
        return trim(xff.substr(0, comma == std::string::npos ? xff.size() : comma));
    }
    return req.remote_ip_address;
}

RestServer::RestServer(SessionManager& mgr,
                       IpToIndexFn ipToIndex,
                       std::string bindAddr,
                       int port)
    : mgr_(mgr), ipToIndex_(std::move(ipToIndex)),
      bind_(std::move(bindAddr)), port_(port) {}

void RestServer::run() {
    crow::SimpleApp app;

    // POST /api/toggle : start session based on caller IP
    CROW_ROUTE(app, "/api/start").methods("POST"_method)([this](const crow::request& req){
        const std::string ip = clientIpFrom(req);

        crow::json::wvalue res;
        res["clientIp"] = ip;

        // check if we have support for this IP
        auto idxOpt = ipToIndex_(ip);
        if (!idxOpt) {
            res["status"] = "error";
            res["error"]  = "no mapping for client IP";
            std::cout << "No config index mapping for client IP " << ip << std::endl;
            return crow::response{400, res};
        }
        const int cfgIndex = *idxOpt;
        res["configIndex"] = cfgIndex;

        // fire-and-forget "start" attempt (idempotency handled by your manager/SimConnect)
        std::string name = "MSFS_" + ip;
        bool ok = mgr_.addSession(static_cast<uint32_t>(cfgIndex), name);

        res["status"] = ok ? "started" : "already-running-or-failed";
        return crow::response{ ok ? 200 : 409, res };
    });

    // stop bridge    
    CROW_ROUTE(app, "/api/stop").methods("POST"_method)([this](const crow::request& req){
        const std::string ip = clientIpFrom(req);

        crow::json::wvalue res;
        res["clientIp"] = ip;
        bool ok = false;
        if (ok = mgr_.stopSessionByIp(ip)) {
            res["status"] = "stopped";
            return crow::response{200, res};
        } else {
            res["status"] = "error";
            res["error"]  = "no running session for client IP";
            return crow::response{400, res};
        }

        res["status"] = ok ? "started" : "already-running-or-failed";
        return crow::response{ ok ? 200 : 409, res };
    });



    app.port(port_).bindaddr(bind_).run();
}
