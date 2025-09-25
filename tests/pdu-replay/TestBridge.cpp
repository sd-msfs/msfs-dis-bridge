#include <iostream>
#include <windows.h>
#include "SimConnect.h"
#include "MappingConfig.h"
#include "Encode.h"
#include "Decode.h"
#include "FlightData.h"

#pragma comment(lib, "SimConnect.lib")

HANDLE hSimConnect = NULL;
DWORD userAircraftObjectID = SIMCONNECT_OBJECT_ID_USER;

// Instances of bridge components
static MappingConfig mappingConfig;
static Encode encoder(mappingConfig);
static Decode decoder(mappingConfig);

// Dispatch callback
void CALLBACK MyDispatchProc(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext) {
    switch (pData->dwID) {
        case SIMCONNECT_RECV_ID_OPEN: {
            std::cout << "[Bridge] Connected to MSFS\n";
            // Request flight data
            SimConnect_AddToDataDefinition(hSimConnect, 1, "GPS POSITION LAT", "degrees latitude");
            SimConnect_AddToDataDefinition(hSimConnect, 1, "GPS POSITION LON", "degrees longitude");
            SimConnect_AddToDataDefinition(hSimConnect, 1, "INDICATED ALTITUDE", "feet");
            SimConnect_AddToDataDefinition(hSimConnect, 1, "ATTITUDE INDICATOR PITCH DEGREES", "degrees");
            SimConnect_AddToDataDefinition(hSimConnect, 1, "ATTITUDE INDICATOR BANK DEGREES", "degrees");
            SimConnect_AddToDataDefinition(hSimConnect, 1, "HEADING INDICATOR", "degrees");
            SimConnect_AddToDataDefinition(hSimConnect, 1, "AIRSPEED INDICATED", "knots");
            SimConnect_RequestDataOnSimObject(hSimConnect, 1, 1, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SECOND);
            break;
        }
        case SIMCONNECT_RECV_ID_SIMOBJECT_DATA: {
            auto* pObjData = (SIMCONNECT_RECV_SIMOBJECT_DATA*)pData;
            if (pObjData->dwRequestID == 1) {
                FlightData* fd = reinterpret_cast<FlightData*>(&pObjData->dwData);
                // Encode to DIS packet
                auto packet = encoder.encodeEvent(*fd);
                // Immediately decode back
                FlightData roundTrip = decoder.decodePacket(packet);
                // Log original vs round-trip
                std::cout << "Original: lat=" << fd->latitude << ", lon=" << fd->longitude << ", alt=" << fd->altitude
                          << ";\nRT: lat=" << roundTrip.latitude << ", lon=" << roundTrip.longitude << ", alt=" << roundTrip.altitude << "\n";
                }
            break;
        }
        case SIMCONNECT_RECV_ID_QUIT: {
            std::cout << "[Bridge] Simulator exit detected. Closing.\n";
            hSimConnect = NULL;
            break;
        }
        default:
            break;
    }
}

int main() {
    HRESULT hr = SimConnect_Open(&hSimConnect, "DIS Gateway Test", nullptr, 0, 0, 0);
    if (FAILED(hr)) {
        std::cerr << "Failed to open SimConnect!\n";
        return -1;
    }

    std::cout << "Waiting for data... Press Ctrl+C to exit.\n";
    while (hSimConnect) {
        SimConnect_CallDispatch(hSimConnect, MyDispatchProc, nullptr);
        Sleep(100);
    }

    SimConnect_Close(hSimConnect);
    return 0;
}
