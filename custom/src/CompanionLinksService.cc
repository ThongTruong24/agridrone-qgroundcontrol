#include "CompanionLinksService.h"

#include <algorithm>
#include <cstddef>

#include "MAVLinkProtocol.h"
#include "Vehicle.h"
#include "VehicleLinkManager.h"

namespace {
template <std::size_t Size>
QString fixedMavlinkString(const char (&value)[Size])
{
    const auto end = std::find(value, value + Size, '\0');
    return QString::fromUtf8(value, static_cast<qsizetype>(end - value));
}
}  // namespace

QVariantMap CompanionLinksService::decode(const mavlink_message_t& message)
{
    mavlink_cc_telemetry_links_t packet{};
    mavlink_msg_cc_telemetry_links_decode(&message, &packet);
    return {
        {QStringLiteral("fc_baudrate"), qulonglong{packet.fc_baudrate}},
        {QStringLiteral("siyi_baudrate"), qulonglong{packet.siyi_baudrate}},
        {QStringLiteral("fc_bytes_rx"), qulonglong{packet.fc_bytes_rx}},
        {QStringLiteral("fc_bytes_tx"), qulonglong{packet.fc_bytes_tx}},
        {QStringLiteral("fc_bitrate_kbps"), packet.fc_bitrate_kbps},
        {QStringLiteral("link_status_flags"), packet.link_status_flags},
        {QStringLiteral("fc_port"), fixedMavlinkString(packet.fc_port)},
        {QStringLiteral("siyi_port"), fixedMavlinkString(packet.siyi_port)},
    };
}

bool CompanionLinksService::sendConfig(Vehicle* vehicle, const mavlink_cc_telemetry_links_t& payload)
{
    if (!vehicle) {
        return false;
    }
    const auto link = vehicle->vehicleLinkManager()->primaryLink().lock();
    if (!link) {
        return false;
    }
    mavlink_message_t message{};
    mavlink_msg_cc_telemetry_links_encode_chan(MAVLinkProtocol::instance()->getSystemId(),
                                               MAVLinkProtocol::getComponentId(), link->mavlinkChannel(), &message,
                                               &payload);
    return vehicle->sendMessageOnLinkThreadSafe(link.get(), message);
}
