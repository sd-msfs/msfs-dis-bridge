#pragma once

#include <string>
#include <unordered_map>
#include <stdexcept>

struct InternalEvent {
    // Legacy field
    std::string name;

    // Newer field
    std::string eventType;

    // Legacy payload map
    std::unordered_map<std::string, double> payload;

    // New-style parameters map
    std::unordered_map<std::string, double> parameters;

    // Constructors
    InternalEvent() = default;
    explicit InternalEvent(const std::string &t) : name(t), eventType(t) {}

    // Convenience setters that keep both maps / names in sync
    void setName(const std::string &n) {
        name = n;
        if (eventType.empty()) eventType = n;
    }
    void setEventType(const std::string &t) {
        eventType = t;
        if (name.empty()) name = t;
    }

    // Write a numeric parameter (keeps both maps in sync)
    void setValue(const std::string &key, double value) {
        payload[key] = value;
        parameters[key] = value;
    }

    // Read a numeric parameter; checks new (parameters) first then legacy (payload)
    double getValue(const std::string &key) const {
        auto it = parameters.find(key);
        if (it != parameters.end()) return it->second;
        it = payload.find(key);
        if (it != payload.end()) return it->second;
        throw std::out_of_range("InternalEvent: key not found: " + key);
    }

    // safe lookup: returns true if parameter exists in either map
    bool has(const std::string &key) const {
        return parameters.find(key) != parameters.end() || payload.find(key) != payload.end();
    }

    // Bulk-sync helper (useful when creating from FlightData)
    void syncMaps() {
        for (auto &kv : payload) parameters.emplace(kv.first, kv.second);
        for (auto &kv : parameters) payload.emplace(kv.first, kv.second);
    }
};
