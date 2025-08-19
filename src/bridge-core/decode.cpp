#include "Decode.h"
#include "MappingConfig.h"
#include <dis6/utils/PduFactory.h>
#include <dis6/utils/DataStream.h>
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

    // Map InternalEvent to FlightData
    return config_.createFlightDataFromEvent(event);
}