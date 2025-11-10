/**
 * @file InternalEvent.h
 * @brief Event strucuture for inter-module communication
 */

#pragma once

#include <string>
#include <unordered_map>

/**
 * @brief A generic event structure to shuttle data between modules
 * The InternalEvent struct provides a standardized way to pass data between
 * different modules in the MSFS DIS Bridge system. It uses a name-based
 * identification system with a flexible payload structure for numeric data.
 *
 * @details This structure is designed to be lightweight and efficient for
 * inter-module communication. The payload uses double precision floating
 * point numbers to accommodate various types of simulation data while
 * maintaining precision.
 *
 * @note The payload is limited to numeric (double) values for simplicity
 * and performance reasons.
 */
struct InternalEvent {
    using Payload = std::unordered_map<std::string, double>;

    /**
     * @brief Name identifier of the event
     *
     * This string identifies the type of event being transmitted.
     * Common examples include "FlightDataUpdate", "SystemStateChange", etc.
     *
     * @note Event names should follow a consistent naming convention
     * across the system for clarity and maintainability.
     */
    std::string name;

    /**
     * @brief Key-value pairs carrying numeric data
     *
     * The payload contains the actual data associated with the event.
     * Keys are descriptive strings identifying the data field, and
     * values are double precision numbers.
     */
    Payload payload;
};
