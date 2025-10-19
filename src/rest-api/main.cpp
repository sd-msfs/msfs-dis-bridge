#include <crow.h>
#include "Encode.h"
#include "Decode.h"
#include "MappingConfig.h"
#include "FlightData.h"

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <windows.h>
#include "SimConnect.h"

#include <algorithm>
#include <filesystem>
#include <vector>
#include <string>
#include <iostream>
#include <mutex>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "SimConnect.lib")

// Base64 encoder
static const std::string base64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

static std::string base64_encode(const std::vector<uint8_t>& bytes) {
    std::string ret;
    int val = 0, valb = -6;
    for (uint8_t c : bytes) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            ret.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) ret.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (ret.size() % 4) ret.push_back('=');
    return ret;
}

// Globals
std::atomic<bool> disBridgeRunning{ false };
std::mutex bridgeMutex;
std::thread bridgeThread;

HANDLE hSimConnect = NULL;
SOCKET udpSocket;
sockaddr_in dest;

static MappingConfig mappingConfig;
static Encode encoder(mappingConfig);
static Decode decoder(mappingConfig);
// Track last sent time for rate limiting
static std::unordered_map<std::string, std::chrono::steady_clock::time_point> lastSent;

// SimConnect Callback
void CALLBACK MyDispatchProc(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext) {
    switch (pData->dwID) {
    case SIMCONNECT_RECV_ID_SIMOBJECT_DATA: {
        auto* pObjData = (SIMCONNECT_RECV_SIMOBJECT_DATA*)pData;
        if (pObjData->dwRequestID == 1) {
            FlightData* fd = reinterpret_cast<FlightData*>(&pObjData->dwData);

            bool sent = false;
            auto* entry = mappingConfig.getMapping("FlightDataUpdate");

            if (entry && entry->enabled) {
                // Rate-limited send
                auto now = std::chrono::steady_clock::now();
                double intervalMs = 1000.0 / std::max(1.0, entry->rateHz);
                auto it = lastSent.find(entry->eventName);

                if (it == lastSent.end() ||
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count() >= intervalMs)
                {
                    lastSent[entry->eventName] = now;

                    auto packet = encoder.encodeEvent(*fd);
                    sendto(udpSocket,
                        reinterpret_cast<const char*>(packet.data()),
                        static_cast<int>(packet.size()), 0,
                        reinterpret_cast<sockaddr*>(&dest),
                        sizeof(dest));

                    // pcapLogger.writePacket(packet);

                    std::cout << "[DIS Bridge] Sent " << entry->pduType
                        << " for event " << entry->eventName
                        << " lat=" << fd->latitude
                        << " lon=" << fd->longitude << std::endl;

                    sent = true;
                }
            }

            if (!sent) {
                // Fallback behavior (no mapping loaded or mapping disabled)
                auto packet = encoder.encodeEvent(*fd);
                sendto(udpSocket,
                    reinterpret_cast<const char*>(packet.data()),
                    static_cast<int>(packet.size()), 0,
                    reinterpret_cast<sockaddr*>(&dest),
                    sizeof(dest));

                std::cout << "[DIS Bridge] (Fallback) lat=" << fd->latitude
                    << " lon=" << fd->longitude << std::endl;
            }
        }
        break;
    }
    default:
        break;
    }
}

// DIS Bridge logic
void run_dis_bridge() {
    // UDP setup
    udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    dest.sin_family = AF_INET;
    dest.sin_port = htons(12345);
    InetPtonA(AF_INET, "127.0.0.1", &dest.sin_addr);

    // SimConnect setup
    HRESULT hr = SimConnect_Open(&hSimConnect, "DIS Bridge", nullptr, 0, 0, 0);
    if (FAILED(hr)) {
        std::cerr << "Failed to open SimConnect" << std::endl;
        disBridgeRunning = false;
        return;
    }

    SimConnect_AddToDataDefinition(hSimConnect, 1, "GPS POSITION LAT", "degrees latitude");
    SimConnect_AddToDataDefinition(hSimConnect, 1, "GPS POSITION LON", "degrees longitude");
    SimConnect_AddToDataDefinition(hSimConnect, 1, "INDICATED ALTITUDE", "feet");
    SimConnect_AddToDataDefinition(hSimConnect, 1, "ATTITUDE INDICATOR PITCH DEGREES", "degrees");
    SimConnect_AddToDataDefinition(hSimConnect, 1, "ATTITUDE INDICATOR BANK DEGREES", "degrees");
    SimConnect_AddToDataDefinition(hSimConnect, 1, "ROTATION VELOCITY BODY Z", "radians per second");
    SimConnect_AddToDataDefinition(hSimConnect, 1, "HEADING INDICATOR", "degrees");
    SimConnect_AddToDataDefinition(hSimConnect, 1, "AIRSPEED INDICATED", "knots");

    SimConnect_RequestDataOnSimObject(hSimConnect, 1, 1, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SECOND);

    std::cout << "[DIS Bridge] Running..." << std::endl;

    // if (!pcapLogger.isOpen()) {
    //     pcapLogger.open("dis_output.pcap");
    // }

    while (disBridgeRunning) {
        SimConnect_CallDispatch(hSimConnect, MyDispatchProc, nullptr);
        Sleep(100);
    }

    closesocket(udpSocket);
    SimConnect_Close(hSimConnect);
    std::cout << "[DIS Bridge] Stopped" << std::endl;

    // pcapLogger.close();
}

int main() {
    crow::SimpleApp app;

#ifdef MAPPINGS_DIR
    const std::string mappingsDir = MAPPINGS_DIR;
#else
    const std::string mappingsDir = "mappings";
#endif

    //auto-load default mapping profile at startup
    try {
        std::filesystem::path defaultProfile = std::filesystem::path(mappingsDir) / "default-mapping.csv";
        if (std::filesystem::exists(defaultProfile)) {
            mappingConfig.loadProfileFromCSV(defaultProfile.string());
            std::cout << "[DIS Bridge] Auto-loaded default-mapping.csv" << std::endl;
        }
        else {
            std::cerr << "[DIS Bridge] Warning: default-mapping.csv not found in "
                << mappingsDir << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[DIS Bridge] Failed to auto-load default-mapping.csv: "
            << e.what() << std::endl;
    }

    CROW_ROUTE(app, "/api/flightdata").methods("POST"_method)(
        [&](const crow::request& req) {
            auto x = crow::json::load(req.body);
            if (!x) return crow::response(400, "Invalid JSON");

            FlightData fd;
            fd.latitude = x["latitude"].d();
            fd.longitude = x["longitude"].d();
            fd.altitude = x["altitude"].d();
            fd.pitch = x["pitch"].d();
            fd.yaw = x["yaw"].d();
            fd.bank = x["bank"].d();
            fd.heading = x["heading"].d();
            fd.airspeed = x["airspeed"].d();

            std::vector<uint8_t> packet;
            try {
                packet = encoder.encodeEvent(fd);
            }
            catch (const std::exception& e) {
                return crow::response(500, e.what());
            }

            crow::json::wvalue res;
            res["packet"] = base64_encode(packet);
            return crow::response{ res };
        }
        );

    CROW_ROUTE(app, "/api/mappingsPath")([&mappingsDir] {
        crow::json::wvalue res;
        res["mappingsPath"] = mappingsDir;
        return crow::response{ res };
        });

    CROW_ROUTE(app, "/api/loadProfile").methods("POST"_method)([&](const crow::request& req) {
        auto x = crow::json::load(req.body);
        if (!x || !x["name"]) return crow::response(400, "Missing 'name' field");

        std::string name = x["name"].s();
        if (name.find("..") != std::string::npos || name.find('/') != std::string::npos || name.find('\\') != std::string::npos) {
            return crow::response(400, "Invalid profile name");
        }

        std::filesystem::path p = std::filesystem::path(mappingsDir) / name;
        if (!std::filesystem::exists(p)) {
            return crow::response(404, "Profile not found");
        }

        try {
            mappingConfig.loadProfileFromCSV(p.string());
            crow::json::wvalue res;
            res["status"] = "ok";
            res["loaded"] = name;
            return crow::response{ res };
        }
        catch (const std::exception& e) {
            return crow::response(500, std::string("failed to load profile: ") + e.what());
        }
        });

    CROW_ROUTE(app, "/api/listProfiles")([&]() {
        crow::json::wvalue res;
        res["profiles"] = crow::json::wvalue::list();
        try {
            if (!std::filesystem::exists(mappingsDir)) {
                return crow::response{ res };
            }
            int i = 0;
            for (auto& entry : std::filesystem::directory_iterator(mappingsDir)) {
                if (!entry.is_regular_file()) continue;
                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".csv") {
                    res["profiles"][i++] = entry.path().filename().string();
                }
            }
            return crow::response{ res };
        }
        catch (const std::exception& e) {
            return crow::response(500, std::string("failed to list profiles: ") + e.what());
        }
        });

    CROW_ROUTE(app, "/api/status")([] {
        crow::json::wvalue res;
        res["status"] = disBridgeRunning ? "started" : "stopped";
        return crow::response{ res };
        });

    CROW_ROUTE(app, "/api/toggle").methods("POST"_method)([] {
        std::lock_guard<std::mutex> lock(bridgeMutex);

        if (!disBridgeRunning) {
            disBridgeRunning = true;
            bridgeThread = std::thread(run_dis_bridge);
            std::cout << "[DIS Bridge] Started" << std::endl;
        }
        else {
            disBridgeRunning = false;
            if (bridgeThread.joinable()) bridgeThread.join();
            std::cout << "[DIS Bridge] Stopped" << std::endl;
        }

        crow::json::wvalue res;
        res["status"] = disBridgeRunning ? "started" : "stopped";
        return crow::response{ res };
        });

    std::cout << "DIS REST API Server running on http://localhost:8080" << std::endl;
    app.port(8080).bindaddr("127.0.0.1").run();
    return 0;
}