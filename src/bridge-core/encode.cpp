#include "Encode.h"
#include "MappingConfig.h"
#include <dis6/EntityStatePdu.h>
#include <iostream>

Encode::Encode(MappingConfig& config)
    : config_(config)
{}

std::vector<uint8_t> Encode::encodeEvent(const FlightData& fd) {
    // Convert raw FlightData to InternalEvent
    InternalEvent event = config_.createEventFromFlightData(fd);

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