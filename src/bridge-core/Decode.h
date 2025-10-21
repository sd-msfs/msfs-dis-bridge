#pragma once

#include <cstdint>   // std::uint8_t
#include <vector>
#include "FlightData.h"

class MappingConfig;

class Decode {
public:
    Decode(MappingConfig& config);
    FlightData decodePacket(const std::vector<uint8_t>& buffer);
    
private:
    MappingConfig& config_;
};