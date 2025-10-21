#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <memory>
#include "FlightData.h"

// Forward declarations
namespace DIS {
    class Pdu;
    class DataStream;
}

struct InternalEvent {
    std::string eventType;
    std::unordered_map<std::string, double> parameters;
};

struct MappingEntry {
    std::string eventName;
    std::string fieldName;
    std::string pduType;
    std::string pduField;
    bool enabled;
    double rateHz; // 0 = event-driven only
    std::chrono::steady_clock::time_point lastSent; // for rate limiting
};

class MappingConfig {
public:
    bool loadProfileFromCSV(const std::string& path);
    const std::vector<MappingEntry>& getMappings() const { return mappings_; }
    
    // Methods required by Encode.cpp and Decode.cpp
    InternalEvent createEventFromFlightData(const FlightData& fd);
    std::unique_ptr<DIS::Pdu> createPduFromEvent(const InternalEvent& event);
    InternalEvent createEventFromPdu(const DIS::Pdu& pdu);
    FlightData createFlightDataFromEvent(const InternalEvent& event);
    
    // Helper method to find a mapping by event name
    const MappingEntry* getMapping(const std::string& eventName) const;

private:
    std::vector<MappingEntry> mappings_;
};