#include <crow.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

// Existing bridge components
#include "Encode.h"
#include "Decode.h"
#include "MappingConfig.h"
#include "FlightData.h"
#include "PcapLogger.h"

// Windows/SimConnect includes
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

// Simple metrics structure (inline to avoid circular dependencies)
struct BridgeMetrics
{
    std::atomic<uint64_t> packets_sent{0};
    std::atomic<uint64_t> packets_received{0};
    std::atomic<uint64_t> bytes_sent{0};
    std::atomic<uint64_t> bytes_received{0};
    std::atomic<uint64_t> pdus_encoded{0};
    std::atomic<uint64_t> pdus_decoded{0};
    std::atomic<uint64_t> encoding_failures{0};
    std::atomic<uint64_t> decoding_failures{0};
    std::atomic<uint64_t> total_errors{0};
    std::atomic<uint64_t> total_warnings{0};

    std::chrono::steady_clock::time_point start_time;
    std::mutex latency_mutex;
    std::vector<double> recent_latencies;
    double avg_latency_ms = 0.0;

    BridgeMetrics() : start_time(std::chrono::steady_clock::now()) {}

    void recordLatency(double latency_ms)
    {
        std::lock_guard<std::mutex> lock(latency_mutex);
        recent_latencies.push_back(latency_ms);

        // Keep only recent 100 measurements
        if (recent_latencies.size() > 100)
        {
            recent_latencies.erase(recent_latencies.begin());
        }

        // Calculate average
        double sum = 0.0;
        for (double lat : recent_latencies)
        {
            sum += lat;
        }
        avg_latency_ms = recent_latencies.empty() ? 0.0 : sum / recent_latencies.size();
    }

    double getUptimeSeconds() const
    {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - start_time).count();
    }
};

// Global metrics instance
BridgeMetrics g_metrics;

// Base64 encoder
static const std::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

static std::string base64_encode(const std::vector<uint8_t> &bytes)
{
    std::string ret;
    int val = 0, valb = -6;
    for (uint8_t c : bytes)
    {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0)
        {
            ret.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6)
        ret.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (ret.size() % 4)
        ret.push_back('=');
    return ret;
}

// Global state for bridge
std::atomic<bool> disBridgeRunning{false};
std::mutex bridgeMutex;
std::thread bridgeThread;

HANDLE hSimConnect = NULL;
SOCKET udpSocket = INVALID_SOCKET;
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
    case SIMCONNECT_RECV_ID_EXCEPTION:
    {
        g_metrics.total_errors++;
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

        dest.sin_family = AF_INET;
        dest.sin_port = htons(12345);
        InetPtonA(AF_INET, "192.168.1.238", &dest.sin_addr);

        // SimConnect setup
        HRESULT hr = SimConnect_Open(&hSimConnect, "DIS Bridge", nullptr, 0, 0, 0);
        if (FAILED(hr))
        {
            throw std::runtime_error("Failed to open SimConnect");
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

        while (disBridgeRunning)
        {
            SimConnect_CallDispatch(hSimConnect, MyDispatchProc, nullptr);
            Sleep(100);
        }
    }
    catch (const std::exception &e)
    {
        g_metrics.total_errors++;
        std::cerr << "[DIS Bridge] Error: " << e.what() << std::endl;
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
    
    while (disBridgeRunning) {
        SimConnect_CallDispatch(hSimConnect, MyDispatchProc, nullptr);
        Sleep(100);
    }

    std::cout << "[DIS Bridge] Stopped" << std::endl;

}

int main()
{
    try
    {
        // Initialize Winsock
        WSADATA wsaData;
        int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (wsaResult != 0)
        {
            std::cerr << "WSAStartup failed: " << wsaResult << std::endl;
            return 1;
        }

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

        // GET /api/v1/metrics - Complete metrics overview
        CROW_ROUTE(app, "/api/v1/metrics")([&]
                                           {
            crow::json::wvalue response;
            
            auto now = std::chrono::system_clock::now();
            response["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                now.time_since_epoch()).count();
            response["uptime_seconds"] = g_metrics.getUptimeSeconds();
            
            // Network metrics
            response["network"]["packets_sent"] = g_metrics.packets_sent.load();
            response["network"]["packets_received"] = g_metrics.packets_received.load();
            response["network"]["bytes_sent"] = g_metrics.bytes_sent.load();
            response["network"]["bytes_received"] = g_metrics.bytes_received.load();
            response["network"]["avg_latency_ms"] = g_metrics.avg_latency_ms;
            
            // Processing metrics
            response["processing"]["pdus_encoded"] = g_metrics.pdus_encoded.load();
            response["processing"]["pdus_decoded"] = g_metrics.pdus_decoded.load();
            response["processing"]["encoding_failures"] = g_metrics.encoding_failures.load();
            response["processing"]["decoding_failures"] = g_metrics.decoding_failures.load();
            
            // Error metrics
            response["errors"]["total_errors"] = g_metrics.total_errors.load();
            response["errors"]["total_warnings"] = g_metrics.total_warnings.load();
            
            // Bridge status
            response["bridge"]["is_running"] = disBridgeRunning.load();
            response["bridge"]["simconnect_connected"] = (hSimConnect != NULL);
            
            return crow::response{response}; });

        // GET /api/v1/metrics/network - Network specific metrics
        CROW_ROUTE(app, "/api/v1/metrics/network")([&]
                                                   {
            crow::json::wvalue response;
            
            response["packets_sent"] = g_metrics.packets_sent.load();
            response["packets_received"] = g_metrics.packets_received.load();
            response["bytes_sent"] = g_metrics.bytes_sent.load();
            response["bytes_received"] = g_metrics.bytes_received.load();
            response["avg_latency_ms"] = g_metrics.avg_latency_ms;
            
            // Calculate packets per second (rough estimate)
            double uptime = g_metrics.getUptimeSeconds();
            response["packets_per_second"] = uptime > 0 ? g_metrics.packets_sent.load() / uptime : 0.0;
            response["bytes_per_second"] = uptime > 0 ? g_metrics.bytes_sent.load() / uptime : 0.0;
            
            return crow::response{response}; });

        // GET /api/v1/metrics/performance - Processing performance metrics
        CROW_ROUTE(app, "/api/v1/metrics/performance")([&]
                                                       {
            crow::json::wvalue response;
            
            response["pdus_encoded"] = g_metrics.pdus_encoded.load();
            response["pdus_decoded"] = g_metrics.pdus_decoded.load();
            response["encoding_failures"] = g_metrics.encoding_failures.load();
            response["decoding_failures"] = g_metrics.decoding_failures.load();
            
            // Calculate success rates
            uint64_t total_encoded = g_metrics.pdus_encoded.load();
            uint64_t encoding_failures = g_metrics.encoding_failures.load();
            uint64_t total_decoded = g_metrics.pdus_decoded.load();
            uint64_t decoding_failures = g_metrics.decoding_failures.load();
            
            response["encoding_success_rate"] = (total_encoded + encoding_failures) > 0 ? 
                (double)total_encoded / (total_encoded + encoding_failures) : 1.0;
            response["decoding_success_rate"] = (total_decoded + decoding_failures) > 0 ? 
                (double)total_decoded / (total_decoded + decoding_failures) : 1.0;
                
            return crow::response{response}; });

        // GET /api/v1/metrics/errors - Error tracking
        CROW_ROUTE(app, "/api/v1/metrics/errors")([&]
                                                  {
            crow::json::wvalue response;
            
            response["total_errors"] = g_metrics.total_errors.load();
            response["total_warnings"] = g_metrics.total_warnings.load();
            response["encoding_failures"] = g_metrics.encoding_failures.load();
            response["decoding_failures"] = g_metrics.decoding_failures.load();
            
            // Error rate per minute
            double uptime_minutes = g_metrics.getUptimeSeconds() / 60.0;
            response["errors_per_minute"] = uptime_minutes > 0 ? g_metrics.total_errors.load() / uptime_minutes : 0.0;
            
            return crow::response{response}; });

        // POST /api/v1/metrics/reset - Reset metrics counters
        CROW_ROUTE(app, "/api/v1/metrics/reset").methods("POST"_method)([&]
                                                                        {
            g_metrics.packets_sent = 0;
            g_metrics.packets_received = 0;
            g_metrics.bytes_sent = 0;
            g_metrics.bytes_received = 0;
            g_metrics.pdus_encoded = 0;
            g_metrics.pdus_decoded = 0;
            g_metrics.encoding_failures = 0;
            g_metrics.decoding_failures = 0;
            g_metrics.total_errors = 0;
            g_metrics.total_warnings = 0;
            
            // Reset latency tracking
            {
                std::lock_guard<std::mutex> lock(g_metrics.latency_mutex);
                g_metrics.recent_latencies.clear();
                g_metrics.avg_latency_ms = 0.0;
            }
            
            // Reset start time
            g_metrics.start_time = std::chrono::steady_clock::now();
            
            crow::json::wvalue response;
            response["message"] = "Metrics reset successfully";
            response["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            return crow::response{response}; });

        // ============ HEALTH ENDPOINTS ============

        // GET /api/v1/health - System health overview
        CROW_ROUTE(app, "/api/v1/health")([&]
                                          {
            crow::json::wvalue response;
            
            // Determine overall health status
            bool is_healthy = true;
            std::string status = "healthy";
            std::vector<std::string> issues;
            
            // Check bridge status
            if (!disBridgeRunning.load()) {
                is_healthy = false;
                issues.push_back("DIS Bridge is not running");
            }
            
            // Check SimConnect status
            if (hSimConnect == NULL && disBridgeRunning.load()) {
                is_healthy = false;
                issues.push_back("SimConnect is not connected");
            }
            
            // Check error rate
            double uptime_minutes = g_metrics.getUptimeSeconds() / 60.0;
            double error_rate = uptime_minutes > 0 ? g_metrics.total_errors.load() / uptime_minutes : 0.0;
            if (error_rate > 5.0) { // More than 5 errors per minute
                is_healthy = false;
                issues.push_back("High error rate: " + std::to_string(error_rate) + " errors/min");
            }
            
            // Check encoding failure rate
            uint64_t total_encoded = g_metrics.pdus_encoded.load();
            uint64_t encoding_failures = g_metrics.encoding_failures.load();
            if (total_encoded > 0) {
                double failure_rate = (double)encoding_failures / (total_encoded + encoding_failures);
                if (failure_rate > 0.05) { // More than 5% failure rate
                    is_healthy = false;
                    issues.push_back("High encoding failure rate: " + std::to_string(failure_rate * 100) + "%");
                }
            }
            
            response["status"] = is_healthy ? "healthy" : "unhealthy";
            response["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            response["uptime_seconds"] = g_metrics.getUptimeSeconds();
            
            // Component status
            response["components"]["dis_bridge"]["status"] = disBridgeRunning.load() ? "healthy" : "stopped";
            response["components"]["simconnect"]["status"] = (hSimConnect != NULL) ? "connected" : "disconnected";
            response["components"]["udp_socket"]["status"] = (udpSocket != INVALID_SOCKET) ? "active" : "inactive";
            
            // Issues array
            crow::json::wvalue issues_array(crow::json::type::List);
            for (size_t i = 0; i < issues.size(); ++i) {
                issues_array[i] = issues[i];
            }
            response["issues"] = std::move(issues_array);
            
            return crow::response{response}; });

        // GET /api/v1/health/components/<component> - Specific component health
        CROW_ROUTE(app, "/api/v1/health/components/<string>")([&](const std::string &component)
                                                              {
            crow::json::wvalue response;
            response["component"] = component;
            response["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            if (component == "dis_bridge" || component == "bridge") {
                response["status"] = disBridgeRunning.load() ? "healthy" : "stopped";
                response["details"]["is_running"] = disBridgeRunning.load();
                response["details"]["uptime_seconds"] = g_metrics.getUptimeSeconds();
            }
            else if (component == "simconnect") {
                bool connected = (hSimConnect != NULL);
                response["status"] = connected ? "connected" : "disconnected";
                response["details"]["is_connected"] = connected;
                if (connected) {
                    response["details"]["data_rate_hz"] = 1.0; // Assuming 1 Hz data rate
                }
            }
            else if (component == "network" || component == "udp") {
                bool active = (udpSocket != INVALID_SOCKET);
                response["status"] = active ? "active" : "inactive";
                response["details"]["socket_active"] = active;
                response["details"]["packets_sent"] = g_metrics.packets_sent.load();
                response["details"]["avg_latency_ms"] = g_metrics.avg_latency_ms;
            }
            else {
                response["status"] = "unknown";
                response["error"] = "Unknown component: " + component;
                return crow::response{404, response};
            }
            
            return crow::response{response}; });

        // ============ LEGACY ENDPOINTS (for backward compatibility) ============

        CROW_ROUTE(app, "/api/status")([&]
                                       {
            crow::json::wvalue res;
            res["status"] = disBridgeRunning ? "started" : "stopped";
            res["uptime_seconds"] = g_metrics.getUptimeSeconds();
            res["packets_sent"] = g_metrics.packets_sent.load();
            res["total_errors"] = g_metrics.total_errors.load();
            return crow::response{res}; });

        CROW_ROUTE(app, "/api/toggle").methods("POST"_method)([&]
                                                              {
            std::lock_guard<std::mutex> lock(bridgeMutex);

            if (!disBridgeRunning) {
                disBridgeRunning = true;
                bridgeThread = std::thread(run_dis_bridge);
                std::cout << "[DIS Bridge] Started" << std::endl;
            } else {
                disBridgeRunning = false;
                if (bridgeThread.joinable()) bridgeThread.join();
                std::cout << "[DIS Bridge] Stopped" << std::endl;
            }

            crow::json::wvalue res;
            res["status"] = disBridgeRunning ? "started" : "stopped";
            res["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            return crow::response{res}; });

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

                FlightData fd;
                fd.latitude = x["latitude"].d();
                fd.longitude = x["longitude"].d();
                fd.altitude = x["altitude"].d();
                fd.pitch = x["pitch"].d();
                fd.bank = x["bank"].d();
                fd.heading = x["heading"].d();
                fd.airspeed = x["airspeed"].d();

                auto start_time = std::chrono::high_resolution_clock::now();
                
                std::vector<uint8_t> packet;
                try {
                    packet = encoder.encodeEvent(fd);
                    g_metrics.pdus_encoded++;
                } catch (const std::exception& e) {
                    g_metrics.encoding_failures++;
                    g_metrics.total_errors++;
                    
                    crow::json::wvalue error;
                    error["error"] = "Encoding failed";
                    error["details"] = e.what();
                    return crow::response{500, error};
                }

                auto end_time = std::chrono::high_resolution_clock::now();
                auto latency = std::chrono::duration<double, std::milli>(end_time - start_time);
                g_metrics.recordLatency(latency.count());

                crow::json::wvalue res;
                res["packet"] = base64_encode(packet);
                res["size_bytes"] = packet.size();
                res["encoding_time_ms"] = latency.count();
                
                g_metrics.packets_sent++; // Count as sent even though it's just encoded
                g_metrics.bytes_sent += packet.size();
                
                return crow::response{res};
                
            } catch (const std::exception& e) {
                g_metrics.total_errors++;
                
                crow::json::wvalue error;
                error["error"] = "Processing failed";
                error["details"] = e.what();
                return crow::response{500, error};
            } });

        // ============ START SERVER ============

        std::cout << "DIS REST API Server running on http://localhost:8080" << std::endl;
        std::cout << "Available endpoints:" << std::endl;
        std::cout << "  GET  /api/v1/metrics          - Complete metrics overview" << std::endl;
        std::cout << "  GET  /api/v1/metrics/network  - Network metrics" << std::endl;
        std::cout << "  GET  /api/v1/metrics/performance - Performance metrics" << std::endl;
        std::cout << "  GET  /api/v1/metrics/errors   - Error metrics" << std::endl;
        std::cout << "  POST /api/v1/metrics/reset    - Reset metrics" << std::endl;
        std::cout << "  GET  /api/v1/health            - System health" << std::endl;
        std::cout << "  GET  /api/v1/health/components/<n> - Component health" << std::endl;
        std::cout << "  POST /api/toggle               - Start/stop bridge" << std::endl;
        std::cout << "  POST /api/flightdata           - Inject flight data" << std::endl;

        app.port(8080).bindaddr("127.0.0.1").multithreaded().run();

        // Cleanup
        WSACleanup();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "DIS REST API Server running on http://localhost:8080" << std::endl;
    app.port(8080).bindaddr("127.0.0.1").run();
    return 0;
}