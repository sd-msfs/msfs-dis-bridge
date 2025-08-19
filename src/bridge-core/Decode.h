#pragma once

#include <vector>
#include <cstdint>
#include "FlightData.h"
#include "InternalEvent.h"

class MappingConfig;

class Decode {
public:
    explicit Decode(MappingConfig& config);
    FlightData decodePacket(const std::vector<uint8_t>& buffer);
private:
    MappingConfig& config_;
};
