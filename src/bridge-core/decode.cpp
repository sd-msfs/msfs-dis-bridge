#include "Decode.h"
#include "MappingConfig.h"
#include <dis6/utils/PduFactory.h>
#include <dis6/utils/DataStream.h>
#include <GeographicLib/Geocentric.hpp>
#include <iostream>

Decode::Decode(MappingConfig& config)
    : config_(config)
{}

FlightData Decode::decodePacket(const std::vector<uint8_t>& buffer) {
    // Create PDU factory instance
    DIS::PduFactory factory;
    
    // Create PDU from buffer
    std::unique_ptr<DIS::Pdu> pdu(
        factory.createPdu(reinterpret_cast<const char*>(buffer.data()))
    );
    
    if (!pdu) {
        throw std::runtime_error("Failed to decode PDU from buffer");
    }

    // Convert PDU to InternalEvent
    InternalEvent event = config_.createEventFromPdu(*pdu);

    //CHECK FOR PDU TYPE HERE
    double lat, lon, alt;
    GeographicLib::Geocentric::WGS84().Reverse(event.parameters["X"], event.parameters["Y"], event.parameters["Z"], lat, lon, alt);
    event.parameters["latitude"] = lat;
    event.parameters["longitude"] = lon;    
    event.parameters["altitude"] = alt;

    // Map InternalEvent to FlightData
    return config_.createFlightDataFromEvent(event);
}