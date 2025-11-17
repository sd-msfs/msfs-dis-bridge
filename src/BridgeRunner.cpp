#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "SessionManager.hpp"
#include "bridge-multicast/UDPMulticaster.hpp"
#include "rest-api/RestServer.hpp"
#include "bridge-core/MappingConfigManager.h"
#include <unordered_map>
#include <filesystem>
#include <iostream>

int main() {
    SessionManager mgr;

    // Initialize MappingConfigManager and load default profile
    auto& configMgr = MappingConfigManager::getInstance();

    // Determine mappings directory
#ifdef MAPPINGS_DIR
    const std::string mappingsDir = MAPPINGS_DIR;
#else
    const std::string mappingsDir = "mappings";
    std::cout << "[BridgeRunner] Using default mappings directory: " << mappingsDir << std::endl;
#endif

    configMgr.setMappingsDir(mappingsDir);

    // Auto-load default mapping profile at startup
    try {
        std::filesystem::path defaultProfile = std::filesystem::path(mappingsDir) / "default-mapping.csv";
        if (std::filesystem::exists(defaultProfile)) {
            configMgr.loadProfileFromCSV(defaultProfile.string());
            std::cout << "[BridgeRunner] Auto-loaded default-mapping.csv" << std::endl;
        } else {
            std::cerr << "[BridgeRunner] Warning: default-mapping.csv not found in "
                      << mappingsDir << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[BridgeRunner] Failed to auto-load default-mapping.csv: "
                  << e.what() << std::endl;
    }

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
