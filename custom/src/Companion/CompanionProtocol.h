#pragma once

#include <QtCore/QString>
#include <cstddef>

#include "LinkInterface.h"
#include "QGCMAVLink.h"

class Vehicle;

/// THACO companion-computer protocol constants and the one place that puts bytes on the wire.
/// Not Vehicle::sendMavCommand*: that requires a Vehicle, but the CC page must still send
/// commands when the FC is down (e.g. to fix the FC UART), when no Vehicle exists.
namespace Companion {

inline constexpr uint8_t kCompId = MAV_COMP_ID_ONBOARD_COMPUTER;

/// param1 of MAV_CMD_THACO_{APPLY,SAVE_DEFAULT,RESTORE_DEFAULT}_CONFIG (thaco_common.xml)
enum Subsystem : int { All = 0, Telemetry = 1, Camera = 2, Network = 3, Vision = 4 };

inline constexpr int kTelemetryTimeoutMs  = 3500;  // CC telemetry is 1 Hz: three missed ticks
inline constexpr int kCommandAckTimeoutMs = 5000;
inline constexpr int kConfirmTimeoutMs    = 6000;  // CC restarts the router before links telemetry resumes
inline constexpr int kToastMs             = 4000;
inline constexpr int kMaxPortBytes = sizeof(mavlink_cc_telemetry_links_t::fc_port) - 1;

template <std::size_t N>
QString fromMavString(const char (&s)[N])
{
    return QString::fromUtf8(s, static_cast<qsizetype>(qstrnlen(s, N)));
}

/// Vehicle primary link, else any connected link (the CC page must work while the FC is down).
SharedLinkInterfacePtr resolveLink(Vehicle* vehicle);
bool sendMessage(Vehicle* vehicle, const SharedLinkInterfacePtr& link, const mavlink_message_t& msg);
bool sendCommand(Vehicle* vehicle, uint8_t targetSystem, MAV_CMD command, float param1, float param2 = 0.0f);

} // namespace Companion
