#include "MappingConfig.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <regex>
#include <dis6/EntityStatePdu.h>
#include <dis6/utils/PduFactory.h>

bool MappingConfig::loadProfileFromCSV(const std::string& path) {
    mappings_.clear();
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open mapping file: " << path << "\n";
        return false;
    }

    std::string line;
    std::getline(file, line); // skip header
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string eventName, fieldName, pduType, pduField, enabledStr, rateStr;

        if (!std::getline(ss, eventName, ',')) continue;
        if (!std::getline(ss, fieldName, ',')) continue;
        if (!std::getline(ss, pduType, ',')) continue;
        if (!std::getline(ss, pduField, ',')) continue;
        if (!std::getline(ss, enabledStr, ',')) enabledStr = "true";
        if (!std::getline(ss, rateStr, ',')) rateStr = "event";

        MappingEntry entry;
        entry.eventName = eventName;
        entry.fieldName = fieldName;
        entry.pduType   = pduType;
        entry.pduField  = pduField;
        entry.enabled   = (enabledStr == "true" || enabledStr == "1");
        
        if (rateStr == "event")
            entry.rateHz = 0.0;
        else
            entry.rateHz = std::stod(rateStr);

        entry.lastSent = std::chrono::steady_clock::now();

        mappings_.push_back(entry);
    }
    return true;
}

const MappingEntry* MappingConfig::getMapping(const std::string& eventName) const {
    for (const auto& entry : mappings_) {
        if (entry.eventName == eventName) {
            return &entry;
        }
    }
    return nullptr;
}

InternalEvent MappingConfig::createEventFromFlightData(const FlightData& fd) {
    InternalEvent event;
    event.eventType = "FlightDataUpdate";
    
    // Map FlightData fields to event parameters
    event.parameters["Latitude"] = fd.latitude;
    event.parameters["Longitude"] = fd.longitude;
    event.parameters["Altitude"] = fd.altitude;
    event.parameters["Pitch"] = fd.pitch;
    event.parameters["Bank"] = fd.bank;
    event.parameters["Heading"] = fd.heading;
    event.parameters["Airspeed"] = fd.airspeed;
    
    return event;
}

std::unique_ptr<DIS::Pdu> MappingConfig::createPduFromEvent(const InternalEvent& event) {
    if (event.eventType == "FlightDataUpdate") {
        auto pdu = std::make_unique<DIS::EntityStatePdu>();
        
        // Set PDU header
        pdu->setProtocolVersion(6);
        pdu->setExerciseID(1);
        pdu->setPduType(1); // Entity State PDU
        
        // Set entity location
        pdu->getEntityLocation().setX(event.parameters.at("Longitude"));
        pdu->getEntityLocation().setY(event.parameters.at("Latitude"));
        pdu->getEntityLocation().setZ(event.parameters.at("Altitude"));
        
        // Set entity orientation
        pdu->getEntityOrientation().setPsi(event.parameters.at("Heading"));
        pdu->getEntityOrientation().setTheta(event.parameters.at("Pitch"));
        pdu->getEntityOrientation().setPhi(event.parameters.at("Bank"));
        
        // Set entity linear velocity
        pdu->getEntityLinearVelocity().setX(event.parameters.at("Airspeed"));
        
        return pdu;
    }
    
    // Handle other event types if needed
    return nullptr;
}

InternalEvent MappingConfig::createEventFromPdu(const DIS::Pdu& pdu) {
    InternalEvent event;
    
    if (pdu.getPduType() == 1) { // Entity State PDU
        const auto& entityPdu = dynamic_cast<const DIS::EntityStatePdu&>(pdu);
        event.eventType = "FlightDataUpdate";
        
        // Extract data from PDU
        event.parameters["Longitude"] = entityPdu.getEntityLocation().getX();
        event.parameters["Latitude"] = entityPdu.getEntityLocation().getY();
        event.parameters["Altitude"] = entityPdu.getEntityLocation().getZ();
        event.parameters["Heading"] = entityPdu.getEntityOrientation().getPsi();
        event.parameters["Pitch"] = entityPdu.getEntityOrientation().getTheta();
        event.parameters["Bank"] = entityPdu.getEntityOrientation().getPhi();
        event.parameters["Airspeed"] = entityPdu.getEntityLinearVelocity().getX();
    }
    
    return event;
}

FlightData MappingConfig::createFlightDataFromEvent(const InternalEvent& event) {
    FlightData fd;
    
    if (event.eventType == "FlightDataUpdate") {
        fd.latitude = event.parameters.at("Latitude");
        fd.longitude = event.parameters.at("Longitude");
        fd.altitude = event.parameters.at("Altitude");
        fd.heading = event.parameters.at("Heading");
        fd.pitch = event.parameters.at("Pitch");
        fd.bank = event.parameters.at("Bank");
        fd.airspeed = event.parameters.at("Airspeed");
    }
    
    return fd;
}