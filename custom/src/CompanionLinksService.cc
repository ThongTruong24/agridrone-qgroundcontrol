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
        {QStringLiteral("fc_tx_rate"), packet.fc_tx_rate},
        {QStringLiteral("fc_rx_rate"), packet.fc_rx_rate},
        {QStringLiteral("fc_tx_rate_max"), packet.fc_tx_rate_max},
        {QStringLiteral("fc_tx_rate_multi"), packet.fc_tx_rate_multi},
        {QStringLiteral("fc_rx_loss"), packet.fc_rx_loss},
        {QStringLiteral("fc_tx_err"), qulonglong{packet.fc_tx_err}},
        {QStringLiteral("siyi_tx_rate"), packet.siyi_tx_rate},
        {QStringLiteral("siyi_rx_rate"), packet.siyi_rx_rate},
        {QStringLiteral("siyi_tx_rate_max"), packet.siyi_tx_rate_max},
        {QStringLiteral("siyi_tx_rate_multi"), packet.siyi_tx_rate_multi},
        {QStringLiteral("siyi_rx_loss"), packet.siyi_rx_loss},
        {QStringLiteral("siyi_tx_err"), qulonglong{packet.siyi_tx_err}},
        {QStringLiteral("siyi_bytes_rx"), qulonglong{packet.siyi_bytes_rx}},
        {QStringLiteral("siyi_bytes_tx"), qulonglong{packet.siyi_bytes_tx}},
        {QStringLiteral("fc_status"), packet.fc_status},
        {QStringLiteral("siyi_status"), packet.siyi_status},
        {QStringLiteral("transport_type"), packet.transport_type},
        {QStringLiteral("fc_port"), fixedMavlinkString(packet.fc_port)},
        {QStringLiteral("siyi_port"), fixedMavlinkString(packet.siyi_port)},
        {QStringLiteral("available_ports"), fixedMavlinkString(packet.available_ports)},
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
