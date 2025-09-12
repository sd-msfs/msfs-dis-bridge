#include <iostream>
#include <string>
#include "SessionManager.hpp"

// -----------------------------------------------------------------------------
// BridgeRunner
//   Entry point for the DIS bridge runner. Spins up multiple SimSessions
//   based on SimConnect.cfg entries (on the bridge machine).
//   Each session maintains its own connection to one MSFS instance.
// -----------------------------------------------------------------------------

int main() {
    SessionManager mgr;

    // -------------------------------------------------------------------------
    // Hard-coded example: try to connect to [SimConnect.0] and [SimConnect.1].
    // You can add as many as you have defined in SimConnect.cfg.
    // -------------------------------------------------------------------------
    int numSessions = 2;  // TODO: set this to the number of MSFS machines
    for (int i = 0; i < numSessions; ++i) {
        std::string name = "MSFS_" + std::to_string(i);
        if (!mgr.addSession(i, name)) {
            std::cerr << "[BridgeRunner] Failed to start session " << i << "\n";
        } else {
            std::cout << "[BridgeRunner] Started session " << name << "\n";
        }
    }

    std::cout << "[BridgeRunner] Bridge running. Press Enter to stop...\n";
    std::cin.get();

    mgr.stopAll();
    std::cout << "[BridgeRunner] Shutdown complete.\n";
    return 0;
}
