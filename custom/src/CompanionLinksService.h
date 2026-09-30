#pragma once

#include <QtCore/QVariantMap>

#include "QGCMAVLink.h"

class Vehicle;

// Handles the LINKS wire format; CompanionController owns reception and lifecycle.
class CompanionLinksService
{
public:
    static QVariantMap decode(const mavlink_message_t& message);
    static bool sendConfig(Vehicle* vehicle, const mavlink_cc_telemetry_links_t& payload);
};
