#pragma once

#include <vector>
#include <cstdint>
#include "FlightData.h"
#include "InternalEvent.h"

class MappingConfig;

class Encode {
public:
    explicit Encode(MappingConfig& config);
    std::vector<uint8_t> encodeEvent(const FlightData& fd);
private:
    MappingConfig& config_;
};
