#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "SessionManager.hpp"
#include "bridge-multicast/UDPMulticaster.hpp"
#include "rest-api/RestServer.hpp"
#include <unordered_map>

int main() {
    SessionManager mgr;

    // start multicast once for all sessions
    UDPMulticaster::getInstance().start("239.1.2.3", 3000, "192.168.1.118", 1, false);

    // simple mapping: 192.168.0.100 -> 0, 192.168.0.101 -> 1
    std::unordered_map<std::string,int> map {
        {"127.0.0.1", 0},
        {"192.168.0.101", 1},
    };
    IpToIndexFn ip2idx = [map](const std::string& ip) -> std::optional<int> {
        if (auto it = map.find(ip); it != map.end()) return it->second;
        return std::nullopt;
    };

    RestServer server(mgr, ip2idx, "127.0.0.1", 8080);
    server.run();  // blocks; POST /api/toggle will start sessions by caller IP
    return 0;
}
