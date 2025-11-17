// --- added: encode + multicast includes ---


#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <SimConnect.h>
#include "SimSession.hpp"
#include <iostream>

#include "Encode.h"
#include "MappingConfig.h"
#include "MappingConfigManager.h"
#include "FlightData.h"
#include "UDPMulticaster.hpp"
#include "services/MetricsCollector.h"
#include <vector>
#include <cstdint>
#include <algorithm>
#include <chrono>
#include <unordered_map>

// Use singleton for shared mapping config across all sessions
static Encode g_encoder(MappingConfigManager::getInstance().getConfig());

const bool doPrint = true; // set to true to enable console logging

// Constructor just stores config index + name
SimSession::SimSession(uint32_t configIndex, const std::string& name)
    : cfgIndex_(configIndex), name_(name) {}

// Destructor makes sure we stop cleanly
SimSession::~SimSession() {
    stop();
}

bool SimSession::start() {
    running_ = true;

    // Record connection attempt
    DISBridge::Services::MetricsCollector::getInstance().recordSimConnectEvent("connection_attempt");

    // Create a Windows event that SimConnect will signal
    evt_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!evt_) {
        std::cerr << "[" << name_ << "] Failed to create event handle\n";
        DISBridge::Services::MetricsCollector::getInstance().recordSimConnectEvent("connection_failure");
        return false;
    }

    // Open SimConnect using the given ConfigIndex
    HRESULT hr = SimConnect_Open(&hSim_, name_.c_str(), nullptr, 0, evt_, cfgIndex_);
    if (hr != S_OK) {
        std::cerr << "[" << name_ << "] SimConnect_Open failed (cfgIndex=" << cfgIndex_ << ")\n";
        DISBridge::Services::MetricsCollector::getInstance().recordSimConnectEvent("connection_failure");
        return false;
    }

    // Record successful connection
    DISBridge::Services::MetricsCollector::getInstance().recordSimConnectEvent("connection_success");

    // Subscribe to pause/sim events
    SimConnect_SubscribeToSystemEvent(hSim_, EVT_PAUSE_EX1, "Pause_EX1");
    SimConnect_SubscribeToSystemEvent(hSim_, EVT_SIM,       "Sim");

    // Register the data definition and request updates
    defineData_();
    requestStream_();

    // Spawn the worker thread
    th_ = std::thread([this] { runLoop_(); });
    return true;
}

void SimSession::stop() {
    running_ = false;

    if (th_.joinable()) th_.join();

    if (hSim_) {
        SimConnect_Close(hSim_);
        hSim_ = nullptr;
    }

    if (evt_) {
        CloseHandle(evt_);
        evt_ = nullptr;
    }
}

// Static dispatch trampoline -> routes into this instance
void CALLBACK SimSession::dispatchThunk(SIMCONNECT_RECV* pData, DWORD cbData, void* ctx) {
    auto self = static_cast<SimSession*>(ctx);
    self->onDispatch_(pData, cbData);
}

// Worker thread waits on event and pumps SimConnect messages
void SimSession::runLoop_() {
    while (running_) {
        DWORD wait = WaitForSingleObject(evt_, 100);
        if (wait == WAIT_OBJECT_0 && hSim_) {
            SimConnect_CallDispatch(hSim_, &dispatchThunk, this);
        }
    }
}

// Define what data we want from MSFS and map it to our SimSample struct
void SimSession::defineData_() {
    SimConnect_AddToDataDefinition(hSim_, DEF_ID, "PLANE LATITUDE", "degrees");
    SimConnect_AddToDataDefinition(hSim_, DEF_ID, "PLANE LONGITUDE", "degrees");
    SimConnect_AddToDataDefinition(hSim_, DEF_ID, "PLANE ALTITUDE", "meters");
    SimConnect_AddToDataDefinition(hSim_, DEF_ID, "PLANE PITCH DEGREES", "degrees");
    SimConnect_AddToDataDefinition(hSim_, DEF_ID, "PLANE BANK DEGREES", "degrees");
    SimConnect_AddToDataDefinition(hSim_, DEF_ID, "PLANE HEADING DEGREES TRUE", "degrees");
    SimConnect_AddToDataDefinition(hSim_, DEF_ID, "VELOCITY BODY X", "meters per second");
    SimConnect_AddToDataDefinition(hSim_, DEF_ID, "VELOCITY BODY Y", "meters per second");
    SimConnect_AddToDataDefinition(hSim_, DEF_ID, "VELOCITY BODY Z", "meters per second");
    SimConnect_AddToDataDefinition(hSim_, DEF_ID, "AIRSPEED INDICATED", "knots");
    SimConnect_AddToDataDefinition(hSim_, DEF_ID, "VERTICAL SPEED", "feet per minute");
    SimConnect_AddToDataDefinition(hSim_, DEF_ID, "GENERAL ENG RPM:1", "percent");
}

// Tell SimConnect: give me this data every simulation frame
// we might want to dynamically reduce this based on overhead???
void SimSession::requestStream_() {
    SimConnect_RequestDataOnSimObject(
        hSim_, REQ_ID, DEF_ID, SIMCONNECT_OBJECT_ID_USER,
        SIMCONNECT_PERIOD_SIM_FRAME, 0, 0, 0, 0);
}

// Handle incoming messages
void SimSession::onDispatch_(SIMCONNECT_RECV* pData, DWORD) {
    switch (pData->dwID) {
    case SIMCONNECT_RECV_ID_EVENT: {
        auto* e = reinterpret_cast<SIMCONNECT_RECV_EVENT*>(pData);
        if (e->uEventID == EVT_PAUSE_EX1) {
            uint32_t flags = static_cast<uint32_t>(e->dwData);
            pauseFlags_.store(flags, std::memory_order_relaxed);
        } else if (e->uEventID == EVT_SIM) {
            // 1 = running, 0 = not running
            simRunning_.store(static_cast<uint32_t>(e->dwData), std::memory_order_relaxed);
        }
        break;
    }

    case SIMCONNECT_RECV_ID_SIMOBJECT_DATA: {
        auto* d = reinterpret_cast<SIMCONNECT_RECV_SIMOBJECT_DATA*>(pData);
        if (d->dwRequestID == REQ_ID && !isPaused()) {
            const SimSample* s = reinterpret_cast<const SimSample*>(&d->dwData);

            // Record data received event for metrics/health
            DISBridge::Services::MetricsCollector::getInstance().recordSimConnectEvent("data_received");

            // print out to console for debugging
            if (doPrint) {
                std::cout << "[" << name_ << "] Lat=" << s->lat
                        << " Lon=" << s->lon
                        << " Alt(m)=" << s->alt
                        << " As(kts)=" << s->airspeed
                        << "\n";
            }

            // --- convert to FlightData expected by your encoder ---
            FlightData fd{};
            fd.latitude  = s->lat;
            fd.longitude = s->lon;
            // Your encoder expected feet in your earlier example; convert from meters:
            fd.altitude  = s->alt * 3.280839895; // m -> ft
            fd.pitch     = s->pitch;
            fd.bank      = s->bank;
            fd.heading   = s->heading;
            fd.airspeed  = s->airspeed;

            // --- encode via your existing mapping/encoder (with timing) ---
            auto encode_start = std::chrono::high_resolution_clock::now();
            std::vector<std::uint8_t> packet = g_encoder.encodeEvent(fd);
            auto encode_end = std::chrono::high_resolution_clock::now();

            // Record encoding time
            auto encode_duration = std::chrono::duration<double, std::milli>(encode_end - encode_start);
            DISBridge::Services::MetricsCollector::getInstance().recordEncodingTime(encode_duration.count());

            // --- enqueue to multicast sender (non-blocking) ---
            UDPMulticaster::getInstance().enqueue(std::move(packet));
        }
        break;
    }

    default:
        break;
    }
}
