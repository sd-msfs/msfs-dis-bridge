#include "Encode.h"
#include "MappingConfig.h"
#include <dis6/EntityStatePdu.h>
#include <GeographicLib/Geocentric.hpp>
#include <iostream>

Encode::Encode(MappingConfig& config)
    : config_(config)
{}

std::vector<uint8_t> Encode::encodeEvent(const FlightData& fd) {
    // Convert raw FlightData to InternalEvent
    InternalEvent event = config_.createEventFromFlightData(fd);

    if(event.eventType == "FlightDataUpdate") {
        double X, Y, Z;
        GeographicLib::Geocentric::WGS84().Forward(fd.latitude, fd.longitude, fd.altitude, X, Y, Z);
        event.parameters["X"] = X;
        event.parameters["Y"] = Y;
        event.parameters["Z"] = Z;        
    }

    // Map InternalEvent → DIS PDU
    std::unique_ptr<DIS::Pdu> pdu = config_.createPduFromEvent(event);
    if (!pdu) {
        throw std::runtime_error("MappingConfig failed to create PDU from event");
    }

    // Create DataStream with default buffer
    DIS::DataStream ds(DIS::BIG);
    
    // Marshal the PDU
    pdu->marshal(ds);
    
    // Access the internal buffer through pointer arithmetic
    const char* buffer_ptr = &ds[0];
    size_t buffer_size = ds.size();
    
    return std::vector<uint8_t>(buffer_ptr, buffer_ptr + buffer_size);
}