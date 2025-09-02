#pragma once
#include "FlightData.h"
#include <vector>

class MappingConfig;

class Encode {
public:
    Encode(MappingConfig& config);
    std::vector<uint8_t> encodeEvent(const FlightData& fd);
    
private:
    MappingConfig& config_;
};