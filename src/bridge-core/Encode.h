#pragma once
#include <cstdint>      // std::uint8_t
#include <vector>
#include "FlightData.h"

// forward-declare to avoid heavy includes here
class MappingConfig;

class Encode {
public:
    explicit Encode(MappingConfig& cfg);

    std::vector<std::uint8_t> encodeEvent(const FlightData& fd);

private:
    MappingConfig& config_;
};
