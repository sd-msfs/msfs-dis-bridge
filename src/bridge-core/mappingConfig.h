#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include "FlightData.h"   // Will's FlightData struct
#include "InternalEvent.h"
#include <dis6/Pdu.h>

class MappingConfig {
public:
    MappingConfig();

    // FlightData ↔ InternalEvent
    InternalEvent createEventFromFlightData(const FlightData& fd) const;
    FlightData   createFlightDataFromEvent(const InternalEvent& event) const;

    // InternalEvent ↔ DIS
    std::unique_ptr<DIS::Pdu> createPduFromEvent(const InternalEvent& event) const;
    InternalEvent              createEventFromPdu(const DIS::Pdu& pdu) const;

private:
    std::unordered_map<std::string, std::string> eventToPduMap_;
    std::unordered_map<std::string, std::string> pduToEventMap_;
};
