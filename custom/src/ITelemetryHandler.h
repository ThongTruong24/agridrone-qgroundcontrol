#pragma once

#include "QGCMAVLink.h"

/**
 * @brief Interface for decoupled MAVLink message handlers (SOLID: Interface Segregation & Dependency Inversion)
 */
class ITelemetryHandler {
public:
    virtual ~ITelemetryHandler() = default;

    /**
     * @brief Process an incoming MAVLink message.
     * @return true if message was handled, false otherwise.
     */
    virtual bool handleMavlinkMessage(const mavlink_message_t& message) = 0;

    /**
     * @brief Reset all internal telemetry states when vehicle disconnects.
     */
    virtual void resetState() = 0;
};
