#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <SimConnect.h>
#include "SimSession.hpp"
#include <iostream>

#include <windows.h>
#include <SimConnect.h>
#include <thread>
#include <atomic>
#include <string>
#include <cstdint>

// -----------------------------------------------------------------------------
// Struct representing the SimConnect data we request from MSFS
// -----------------------------------------------------------------------------
#pragma pack(push, 1)
struct SimSample {
    double lat;      // PLANE LATITUDE
    double lon;      // PLANE LONGITUDE
    double alt;      // PLANE ALTITUDE (meters)
    double pitch;    // PLANE PITCH DEGREES
    double bank;     // PLANE BANK DEGREES
    double heading;  // PLANE HEADING DEGREES TRUE   <-- ADD THIS
    double velX;     // VELOCITY BODY X
    double velY;     // VELOCITY BODY Y
    double velZ;     // VELOCITY BODY Z
    double airspeed; // AIRSPEED INDICATED
    double vs;       // VERTICAL SPEED
    double engRpm;   // GENERAL ENG RPM:1
};
#pragma pack(pop)

// -----------------------------------------------------------------------------
// SimSession = one independent SimConnect connection to one MSFS instance
// -----------------------------------------------------------------------------
class SimSession {
public:
    SimSession(uint32_t configIndex, const std::string& name);
    ~SimSession();

    bool start();
    void stop();

    // Pause state helpers
    bool isPaused() const { return pauseFlags_.load(std::memory_order_relaxed) != 0; }
    uint32_t pauseFlags() const { return pauseFlags_.load(std::memory_order_relaxed); }
    bool isSimRunning() const { return simRunning_.load(std::memory_order_relaxed) != 0; }
    std::string getName() const { return name_; }

private:
    static void CALLBACK dispatchThunk(SIMCONNECT_RECV* pData, DWORD cbData, void* ctx);
    void runLoop_();
    void defineData_();
    void requestStream_();
    void onDispatch_(SIMCONNECT_RECV* pData, DWORD cbData);

    enum { DEF_ID = 1, REQ_ID = 100 };
    enum : DWORD { EVT_PAUSE_EX1 = 1, EVT_SIM = 2 };

    uint32_t cfgIndex_;
    std::string name_;

    HANDLE evt_{nullptr};
    HANDLE hSim_{nullptr};
    std::thread th_;
    std::atomic<bool> running_{false};

    std::atomic<uint32_t> pauseFlags_{0};
    std::atomic<uint32_t> simRunning_{0};
};
