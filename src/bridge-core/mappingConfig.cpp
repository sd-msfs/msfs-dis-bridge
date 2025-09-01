#include "MappingConfig.h"
#include <dis6/EntityStatePdu.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <typeinfo>
#include <cstring>

static inline std::string trim(const std::string& s) {
    auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

MappingConfig::MappingConfig() {
    // Default hard-coded mapping (if no profile loaded)
    eventToPduMap_["FlightDataUpdate"] = "EntityStatePdu";
    pduToEventMap_["EntityStatePdu"] = "FlightDataUpdate";
}

bool MappingConfig::loadProfileFromCSV(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("MappingConfig: cannot open file: " + path);
    }

    std::vector<MappingRule> tmp;
    std::string line;
    size_t lineno = 0;
    while (std::getline(in, line)) {
        lineno++;
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;

        std::stringstream ss(line);
        std::string simField, payloadKey, scaleS, offsetS;

        // Read up to 4 comma-separated fields
        if (!std::getline(ss, simField, ',')) {
            throw std::runtime_error("MappingConfig: malformed CSV at line " + std::to_string(lineno));
        }
        if (!std::getline(ss, payloadKey, ',')) {
            throw std::runtime_error("MappingConfig: malformed CSV at line " + std::to_string(lineno));
        }
        // optional scale
        if (!std::getline(ss, scaleS, ',')) scaleS = "1.0";
        // optional offset
        if (!std::getline(ss, offsetS, ',')) offsetS = "0.0";

        MappingRule r;
        r.simField = trim(simField);
        r.payloadKey = trim(payloadKey);
        if (r.simField.empty() || r.payloadKey.empty()) {
            throw std::runtime_error("MappingConfig: empty simField or payloadKey at line " + std::to_string(lineno));
        }
        try {
            r.scale = std::stod(trim(scaleS));
        } catch (...) { r.scale = 1.0; }
        try {
            r.offset = std::stod(trim(offsetS));
        } catch (...) { r.offset = 0.0; }

        tmp.push_back(r);
    }

    
    // Basic validation: ensure simField names are known
    for (const auto &r : tmp) {
        if (r.simField != "latitude" &&
            r.simField != "longitude" &&
            r.simField != "altitude" &&
            r.simField != "pitch" &&
            r.simField != "bank" &&
            r.simField != "heading" &&
            r.simField != "airspeed") {
            throw std::runtime_error("MappingConfig: unknown simField '" + r.simField + "'");
        }
    }

    // If parse & validation succeeded, atomically swap into active rules
    {
        std::lock_guard<std::mutex> lk(rulesMutex_);
        rules_.swap(tmp);
    }

    return true;
}

double MappingConfig::getFieldValue(const FlightData& fd, const std::string& fieldName) {
    if (fieldName == "latitude") return fd.latitude;
    if (fieldName == "longitude") return fd.longitude;
    if (fieldName == "altitude") return fd.altitude;
    if (fieldName == "pitch") return fd.pitch;
    if (fieldName == "bank") return fd.bank;
    if (fieldName == "heading") return fd.heading;
    if (fieldName == "airspeed") return fd.airspeed;
    throw std::runtime_error("MappingConfig::getFieldValue: unknown field '" + fieldName + "'");
}

// Build InternalEvent from FlightData using loaded mapping rules
InternalEvent MappingConfig::createEventFromFlightData(const FlightData& fd) const {
    InternalEvent ev;
    ev.name = "FlightDataUpdate";
    ev.payload.clear();

    // read rules under lock (copy snapshot for thread-safety)
    std::vector<MappingRule> snapshot;
    {
        std::lock_guard<std::mutex> lk(rulesMutex_);
        snapshot = rules_;
    }

    if (snapshot.empty()) {
        // fallback: original hard-coded behavior
        ev.payload["latitude"]  = fd.latitude;
        ev.payload["longitude"] = fd.longitude;
        ev.payload["altitude"]  = fd.altitude;
        ev.payload["pitch"]     = fd.pitch;
        ev.payload["bank"]      = fd.bank;
        ev.payload["heading"]   = fd.heading;
        ev.payload["airspeed"]  = fd.airspeed;
        return ev;
    }

    // Apply rules
    for (const auto &r : snapshot) {
        double simVal = getFieldValue(fd, r.simField);
        double mapped = simVal * r.scale + r.offset;
        ev.payload[r.payloadKey] = mapped;
    }

    return ev;
}

// Existing createPduFromEvent — unchanged logically, but it will use event.payload keys
std::unique_ptr<DIS::Pdu> MappingConfig::createPduFromEvent(const InternalEvent& event) const {
    auto it = eventToPduMap_.find(event.name);
    if (it == eventToPduMap_.end()) return nullptr;

    if (it->second == "EntityStatePdu") {
        auto pdu = std::make_unique<DIS::EntityStatePdu>();
        const auto& pl = event.payload;

        // Using at() will throw if required keys are missing.
        DIS::Vector3Double location;
        location.setX(pl.at("latitude"));
        location.setY(pl.at("longitude"));
        location.setZ(pl.at("altitude"));

        DIS::Orientation orientation;
        orientation.setPhi(static_cast<float>(pl.at("pitch")));
        orientation.setTheta(static_cast<float>(pl.at("bank")));
        orientation.setPsi(static_cast<float>(pl.at("heading")));

        pdu->setEntityLocation(location);
        pdu->setEntityOrientation(orientation);

        return pdu;
    }
    return nullptr;
}

// createEventFromPdu & createFlightDataFromEvent remain similar to your earlier code
InternalEvent MappingConfig::createEventFromPdu(const DIS::Pdu& pdu) const {
    unsigned char pduType = pdu.getPduType();
    std::string type;

    switch (pduType) {
        case 1: type = "EntityStatePdu"; break;
        default: {
            const char* className = typeid(pdu).name();
            if (strstr(className, "EntityStatePdu")) {
                type = "EntityStatePdu";
            } else {
                type = "Unknown";
            }
        }
    }

    auto it = pduToEventMap_.find(type);
    if (it == pduToEventMap_.end()) return {};

    InternalEvent ev;
    ev.name = it->second;
    if (type == "EntityStatePdu") {
        const auto& esp = static_cast<const DIS::EntityStatePdu&>(pdu);
        auto& pl = ev.payload;

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
