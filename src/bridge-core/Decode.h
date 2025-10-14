#pragma once
#include "FlightData.h"
#include <vector>

class MappingConfig;

class Decode {
public:
    Decode(MappingConfig& config);
    FlightData decodePacket(const std::vector<uint8_t>& buffer);
    
private:
    MappingConfig& config_;
};