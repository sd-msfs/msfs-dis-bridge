#include "MappingConfig.h"
#include <dis6/EntityStatePdu.h>
#include <stdexcept>

MappingConfig::MappingConfig() {
    // Hard-code one mapping: FlightData events ↔ EntityStatePdu
    eventToPduMap_["FlightDataUpdate"] = "EntityStatePdu";
    pduToEventMap_["EntityStatePdu"] = "FlightDataUpdate";
}

InternalEvent MappingConfig::createEventFromFlightData(const FlightData& fd) const {
    InternalEvent ev;
    ev.name = "FlightDataUpdate";
    ev.payload.clear();
    ev.payload["latitude"]  = fd.latitude;
    ev.payload["longitude"] = fd.longitude;
    ev.payload["altitude"]  = fd.altitude;
    ev.payload["pitch"]     = fd.pitch;
    ev.payload["bank"]      = fd.bank;
    ev.payload["heading"]   = fd.heading;
    ev.payload["airspeed"]  = fd.airspeed;
    return ev;
}

std::unique_ptr<DIS::Pdu> MappingConfig::createPduFromEvent(const InternalEvent& event) const {
    auto it = eventToPduMap_.find(event.name);
    if (it == eventToPduMap_.end()) return nullptr;

    if (it->second == "EntityStatePdu") {
        auto pdu = std::make_unique<DIS::EntityStatePdu>();
        const auto& pl = event.payload;
        
        // Create location and orientation objects
        DIS::Vector3Double location;
        location.setX(pl.at("latitude"));
        location.setY(pl.at("longitude"));
        location.setZ(pl.at("altitude"));
        
        DIS::Orientation orientation;
        orientation.setPhi(static_cast<float>(pl.at("pitch")));
        orientation.setTheta(static_cast<float>(pl.at("bank")));
        orientation.setPsi(static_cast<float>(pl.at("heading")));
        
        // Set values in PDU
        pdu->setEntityLocation(location);
        pdu->setEntityOrientation(orientation);
        
        return pdu;
    }
    return nullptr;
}

InternalEvent MappingConfig::createEventFromPdu(const DIS::Pdu& pdu) const {
    // Get PDU type
    unsigned char pduType = pdu.getPduType();
    std::string type;
    
    // Map PDU type to string
    switch(pduType) {
        case 1: type = "EntityStatePdu"; break;
        // Add other PDU types as needed
        default: 
            // Use a fallback method to get class name
            const char* className = typeid(pdu).name();
            // Demangle if needed (simplified for Windows)
            if (strstr(className, "EntityStatePdu")) {
                type = "EntityStatePdu";
            } else {
                type = "Unknown";
            }
    }
    
    auto it = pduToEventMap_.find(type);
    if (it == pduToEventMap_.end()) return {};

    InternalEvent ev;
    ev.name = it->second;
    if (type == "EntityStatePdu") {
        const auto& esp = static_cast<const DIS::EntityStatePdu&>(pdu);
        auto& pl = ev.payload;
        
        // Get location and orientation
        DIS::Vector3Double location = esp.getEntityLocation();
        DIS::Orientation orient = esp.getEntityOrientation();
        
        pl["latitude"]  = location.getX();
        pl["longitude"] = location.getY();
        pl["altitude"]  = location.getZ();
        pl["pitch"]     = orient.getPhi();
        pl["bank"]      = orient.getTheta();
        pl["heading"]   = orient.getPsi();
    }
    return ev;
}

FlightData MappingConfig::createFlightDataFromEvent(const InternalEvent& event) const {
    FlightData fd{};
    const auto& pl = event.payload;
    fd.latitude  = pl.at("latitude");
    fd.longitude = pl.at("longitude");
    fd.altitude  = pl.at("altitude");
    fd.pitch     = pl.at("pitch");
    fd.bank      = pl.at("bank");
    fd.heading   = pl.at("heading");
    fd.airspeed  = pl.count("airspeed") ? pl.at("airspeed") : 0.0;
    return fd;
}