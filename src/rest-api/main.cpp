#include <crow.h>
#include "Encode.h"
#include "Decode.h"
#include "MappingConfig.h"
#include "FlightData.h"

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <windows.h>
#include "SimConnect.h"

#include <vector>
#include <string>
#include <iostream>
#include <mutex>
#include <thread>
#include <atomic>

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

// SimConnect Callback
void CALLBACK MyDispatchProc(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext) {
    switch (pData->dwID) {
    case SIMCONNECT_RECV_ID_SIMOBJECT_DATA: {
        auto* pObjData = (SIMCONNECT_RECV_SIMOBJECT_DATA*)pData;
        if (pObjData->dwRequestID == 1) {
            FlightData* fd = reinterpret_cast<FlightData*>(&pObjData->dwData);
            auto packet = encoder.encodeEvent(*fd);

            // Send over UDP
            sendto(udpSocket,
                reinterpret_cast<const char*>(packet.data()),
                static_cast<int>(packet.size()), 0,
                reinterpret_cast<sockaddr*>(&dest),
                sizeof(dest));

            std::cout << "[DIS Bridge] lat=" << fd->latitude
                << " lon=" << fd->longitude << std::endl;
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
    InetPtonA(AF_INET, "192.168.1.238", &dest.sin_addr);

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
    SimConnect_AddToDataDefinition(hSimConnect, 1, "HEADING INDICATOR", "degrees");
    SimConnect_AddToDataDefinition(hSimConnect, 1, "AIRSPEED INDICATED", "knots");

    SimConnect_RequestDataOnSimObject(hSimConnect, 1, 1, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SECOND);

    std::cout << "[DIS Bridge] Running..." << std::endl;

    while (disBridgeRunning) {
        SimConnect_CallDispatch(hSimConnect, MyDispatchProc, nullptr);
        Sleep(100);
    }

    closesocket(udpSocket);
    SimConnect_Close(hSimConnect);
    std::cout << "[DIS Bridge] Stopped" << std::endl;
}

int main() {
    crow::SimpleApp app;

    CROW_ROUTE(app, "/api/flightdata").methods("POST"_method)(
        [&](const crow::request& req) {
            auto x = crow::json::load(req.body);
            if (!x) return crow::response(400, "Invalid JSON");

            FlightData fd;
            fd.latitude = x["latitude"].d();
            fd.longitude = x["longitude"].d();
            fd.altitude = x["altitude"].d();
            fd.pitch = x["pitch"].d();
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
    app.port(8080).bindaddr("127.0.0.1").multithreaded().run();
    return 0;
}
