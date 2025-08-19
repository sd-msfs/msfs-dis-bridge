#pragma once

#include <string>
#include <unordered_map>

// A generic event structure to shuttle data between modules
struct InternalEvent {
    using Payload = std::unordered_map<std::string, double>;

    // Name of the event, e.g., "FlightDataUpdate"
    std::string name;

    // Key/value pairs carrying numeric data
    Payload payload;
};
