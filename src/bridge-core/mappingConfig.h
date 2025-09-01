#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <mutex>
#include <stdexcept>
#include "FlightData.h"
#include "InternalEvent.h"
#include <dis6/Pdu.h>

class MappingConfig {
public:
    MappingConfig();

    // Load a mapping profile from a CSV file.
    // Throws std::runtime_error on parse/validation errors.
    bool loadProfileFromCSV(const std::string& path);

    // FlightData ↔ InternalEvent
    InternalEvent createEventFromFlightData(const FlightData& fd) const;
    FlightData   createFlightDataFromEvent(const InternalEvent& event) const;

    // InternalEvent ↔ DIS
    std::unique_ptr<DIS::Pdu> createPduFromEvent(const InternalEvent& event) const;
    InternalEvent              createEventFromPdu(const DIS::Pdu& pdu) const;

private:
    struct MappingRule {
        std::string simField;   // e.g. "latitude"
        std::string payloadKey; // e.g. "latitude" (used in InternalEvent.payload)
        double scale = 1.0;
        double offset = 0.0;
    };

    // Active mapping rules (apply in order)
    std::vector<MappingRule> rules_;
    mutable std::mutex rulesMutex_;

    // fallback hard-coded maps for PDU type names (keeps original behavior)
    std::unordered_map<std::string, std::string> eventToPduMap_;
    std::unordered_map<std::string, std::string> pduToEventMap_;

    // helper: extract named field value from FlightData
    static double getFieldValue(const FlightData& fd, const std::string& fieldName);
};
