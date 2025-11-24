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

    // Only include fields that are enabled in the mappings configuration
    for (const auto& mapping : mappings_) {
        if (!mapping.enabled) continue;
        if (mapping.eventName != "FlightDataUpdate") continue;

        // Map FlightData fields to event parameters based on CSV configuration
        if (mapping.fieldName == "Latitude") {
            event.parameters["Latitude"] = fd.latitude;
        } else if (mapping.fieldName == "Longitude") {
            event.parameters["Longitude"] = fd.longitude;
        } else if (mapping.fieldName == "Altitude") {
            event.parameters["Altitude"] = fd.altitude;
        } else if (mapping.fieldName == "Pitch") {
            event.parameters["Pitch"] = fd.pitch;
        } else if (mapping.fieldName == "Bank" || mapping.fieldName == "Roll") {
            event.parameters[mapping.fieldName] = fd.bank;
        } else if (mapping.fieldName == "Heading") {
            event.parameters["Heading"] = fd.heading;
        } else if (mapping.fieldName == "Airspeed") {
            event.parameters["Airspeed"] = fd.airspeed;
        } else if (mapping.fieldName == "VelocityX") {
            event.parameters["VelocityX"] = fd.velX;
        } else if (mapping.fieldName == "VelocityY") {
            event.parameters["VelocityY"] = fd.velY;
        } else if (mapping.fieldName == "VelocityZ") {
            event.parameters["VelocityZ"] = fd.velZ;
        }
    }

    return event;
}

std::unique_ptr<DIS::Pdu> MappingConfig::createPduFromEvent(const InternalEvent& event) {
    if (event.eventType == "FlightDataUpdate") {
        auto pdu = std::make_unique<DIS::EntityStatePdu>();

        // Set PDU header
        pdu->setProtocolVersion(6);
        pdu->setExerciseID(1);
        pdu->setPduType(1); // Entity State PDU

        // Only set PDU fields that are present in the event parameters
        // Set entity location (only if fields are in the event)
        if (event.parameters.find("Longitude") != event.parameters.end()) {
            pdu->getEntityLocation().setX(event.parameters.at("Longitude"));
        }
        if (event.parameters.find("Latitude") != event.parameters.end()) {
            pdu->getEntityLocation().setY(event.parameters.at("Latitude"));
        }
        if (event.parameters.find("Altitude") != event.parameters.end()) {
            pdu->getEntityLocation().setZ(event.parameters.at("Altitude"));
        }

        // Set entity orientation (only if fields are in the event)
        if (event.parameters.find("Heading") != event.parameters.end()) {
            pdu->getEntityOrientation().setPsi(event.parameters.at("Heading"));
        }
        if (event.parameters.find("Pitch") != event.parameters.end()) {
            pdu->getEntityOrientation().setTheta(event.parameters.at("Pitch"));
        }
        if (event.parameters.find("Bank") != event.parameters.end()) {
            pdu->getEntityOrientation().setPhi(event.parameters.at("Bank"));
        } else if (event.parameters.find("Roll") != event.parameters.end()) {
            pdu->getEntityOrientation().setPhi(event.parameters.at("Roll"));
        }

        // Set entity linear velocity (only if fields are in the event)
        if (event.parameters.find("Airspeed") != event.parameters.end()) {
            pdu->getEntityLinearVelocity().setX(event.parameters.at("Airspeed"));
        }
        if (event.parameters.find("VelocityX") != event.parameters.end()) {
            pdu->getEntityLinearVelocity().setX(event.parameters.at("VelocityX"));
        }
        if (event.parameters.find("VelocityY") != event.parameters.end()) {
            pdu->getEntityLinearVelocity().setY(event.parameters.at("VelocityY"));
        }
        if (event.parameters.find("VelocityZ") != event.parameters.end()) {
            pdu->getEntityLinearVelocity().setZ(event.parameters.at("VelocityZ"));
        }

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

        // Only extract fields that are enabled in the mappings configuration
        for (const auto& mapping : mappings_) {
            if (!mapping.enabled) continue;
            if (mapping.eventName != "FlightDataUpdate") continue;

            // Extract PDU fields based on CSV configuration
            if (mapping.fieldName == "Longitude") {
                event.parameters["Longitude"] = entityPdu.getEntityLocation().getX();
            } else if (mapping.fieldName == "Latitude") {
                event.parameters["Latitude"] = entityPdu.getEntityLocation().getY();
            } else if (mapping.fieldName == "Altitude") {
                event.parameters["Altitude"] = entityPdu.getEntityLocation().getZ();
            } else if (mapping.fieldName == "Heading") {
                event.parameters["Heading"] = entityPdu.getEntityOrientation().getPsi();
            } else if (mapping.fieldName == "Pitch") {
                event.parameters["Pitch"] = entityPdu.getEntityOrientation().getTheta();
            } else if (mapping.fieldName == "Bank" || mapping.fieldName == "Roll") {
                // Both "Bank" and "Roll" refer to phi
                event.parameters[mapping.fieldName] = entityPdu.getEntityOrientation().getPhi();
            } else if (mapping.fieldName == "Airspeed" || mapping.fieldName == "VelocityX") {
                event.parameters[mapping.fieldName] = entityPdu.getEntityLinearVelocity().getX();
            } else if (mapping.fieldName == "VelocityY") {
                event.parameters["VelocityY"] = entityPdu.getEntityLinearVelocity().getY();
            } else if (mapping.fieldName == "VelocityZ") {
                event.parameters["VelocityZ"] = entityPdu.getEntityLinearVelocity().getZ();
            }
        }
    }

    return event;
}

FlightData MappingConfig::createFlightDataFromEvent(const InternalEvent& event) {
    FlightData fd = {}; // Initialize all fields to 0

    if (event.eventType == "FlightDataUpdate") {
        // Only populate FlightData fields that are present in the event
        auto it = event.parameters.find("Latitude");
        if (it != event.parameters.end()) fd.latitude = it->second;

        it = event.parameters.find("Longitude");
        if (it != event.parameters.end()) fd.longitude = it->second;

        it = event.parameters.find("Altitude");
        if (it != event.parameters.end()) fd.altitude = it->second;

        it = event.parameters.find("Heading");
        if (it != event.parameters.end()) fd.heading = it->second;

        it = event.parameters.find("Pitch");
        if (it != event.parameters.end()) fd.pitch = it->second;

        it = event.parameters.find("Bank");
        if (it != event.parameters.end()) fd.bank = it->second;

        // Also check for "Roll" (synonym for Bank)
        it = event.parameters.find("Roll");
        if (it != event.parameters.end()) fd.bank = it->second;

        it = event.parameters.find("Airspeed");
        if (it != event.parameters.end()) fd.airspeed = it->second;

        it = event.parameters.find("VelocityX");
        if (it != event.parameters.end()) fd.velX = it->second;

        it = event.parameters.find("VelocityY");
        if (it != event.parameters.end()) fd.velY = it->second;

        it = event.parameters.find("VelocityZ");
        if (it != event.parameters.end()) fd.velZ = it->second;
    }

    return fd;
}