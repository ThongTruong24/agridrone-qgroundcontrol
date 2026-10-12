#include "CompanionLinksService.h"
#include "MAVLinkProtocol.h"
#include "MultiVehicleManager.h"
#include "LinkManager.h"
#include "VehicleLinkManager.h"
#include <QtCore/QDebug>

CompanionLinksService::CompanionLinksService(QObject* parent)
    : QObject(parent)
{
    _clock.start();
    resetState();
}

void CompanionLinksService::resetState()
{
    _indexedLinks.clear();
    _receivedAt.clear();
    _hasLinksTelemetry = false;
    _fcBaud = 0;
    _siyiBaud = 0;
    _fcPort.clear();
    _siyiPort.clear();
    _transportProtocol = QStringLiteral("Serial / UART");
    _fcStatus = 0;
    _siyiStatus = 0;
    _fcBitrateKbps = 0.0f;
    _fcPacketDropRate = 0.0f;
    _fcBytesRx = 0;
    _fcBytesTx = 0;
    _siyiBytesRx = 0;
    _siyiBytesTx = 0;
    _siyiLinkQuality = 0;
    _linkStatusFlags = 0;

    _fcTxRate = 0.0f;
    _fcRxRate = 0.0f;
    _fcTxRateMax = 0.0f;
    _fcTxRateMulti = 0.0f;
    _fcRxLoss = 0.0f;
    _fcTxErr = 0;

    _siyiTxRate = 0.0f;
    _siyiRxRate = 0.0f;
    _siyiTxRateMax = 0.0f;
    _siyiTxRateMulti = 0.0f;
    _siyiRxLoss = 0.0f;
    _siyiTxErr = 0;

    _availablePorts.clear();
    _prevFcStatus = -1;
    _prevSiyiStatus = -1;

    emit linksChanged();
    emit availablePortsChanged();
}

bool CompanionLinksService::handleMavlinkMessage(const mavlink_message_t& message)
{
    if (message.msgid != 42010 && message.msgid != MAVLINK_MSG_ID_CC_SERIAL_LINK) {
        return false;
    }

    // Accept only from Companion Computer component ID (191) or broadcast (0)
    if (message.compid != 191 && message.compid != 0) {
        return false;
    }

    mavlink_cc_serial_link_t lnk;
    mavlink_msg_cc_serial_link_decode(&message, &lnk);

    _hasLinksTelemetry = true;
    QString linkName = QString::fromUtf8(lnk.name, qstrnlen(lnk.name, sizeof(lnk.name))).trimmed();
    QString portStr = QString::fromUtf8(lnk.port, qstrnlen(lnk.port, sizeof(lnk.port))).trimmed();

    _indexedLinks[lnk.link_index] = {{"index", lnk.link_index}, {"name", linkName}, {"port", portStr},
        {"status", lnk.status}, {"baudrate", lnk.baudrate}, {"txRate", lnk.tx_rate}, {"rxRate", lnk.rx_rate}};
    _receivedAt[lnk.link_index] = _clock.elapsed();

    // Drop links the Companion no longer reports (link_count shrank).
    for (auto it = _indexedLinks.begin(); lnk.link_count > 0 && it != _indexedLinks.end();) {
        if (it.key() >= lnk.link_count) {
            _receivedAt.remove(it.key());
            it = _indexedLinks.erase(it);
        } else {
            ++it;
        }
    }

    if (lnk.link_index == 0) {
        _fcBaud = static_cast<int>(lnk.baudrate);
        _fcPort = portStr;
        _fcStatus = lnk.status;
        _fcTxRate = lnk.tx_rate;
        _fcRxRate = lnk.rx_rate;
        _fcRxLoss = lnk.rx_loss;
        _fcTxErr = lnk.rx_errors;
        _fcBytesRx = lnk.rx_bytes;
        _fcBytesTx = lnk.tx_bytes;
        _fcBitrateKbps = (_fcRxRate * 8.0f) / 1000.0f;
        _fcPacketDropRate = _fcRxLoss;

        if (_fcStatus != _prevFcStatus) {
            emit logMessage(QStringLiteral("FC"), QStringLiteral("INFO"),
                             QStringLiteral("[EVENT] FC Link Status -> %1 (%2@%3 bps)")
                                 .arg(_fcStatus == 2 ? "ONLINE" : (_fcStatus == 1 ? "STANDBY" : "OFFLINE"))
                                 .arg(_fcPort).arg(_fcBaud),
                             _fcStatus == 2 ? 6 : 4);
            _prevFcStatus = _fcStatus;
        }
    } else if (lnk.link_index == 1) {
        _siyiBaud = static_cast<int>(lnk.baudrate);
        _siyiPort = portStr;
        _siyiStatus = lnk.status;
        _siyiTxRate = lnk.tx_rate;
        _siyiRxRate = lnk.rx_rate;
        _siyiRxLoss = lnk.rx_loss;
        _siyiTxErr = lnk.rx_errors;
        _siyiBytesRx = lnk.rx_bytes;
        _siyiBytesTx = lnk.tx_bytes;
        _siyiBitrateKbps = (_siyiRxRate * 8.0f) / 1000.0f;
        _siyiPacketDropRate = _siyiRxLoss;
        _siyiLinkQuality = (_siyiRxLoss < 100.0f) ? static_cast<int>(100.0f - _siyiRxLoss) : 0;

        if (_siyiStatus != _prevSiyiStatus) {
            emit logMessage(QStringLiteral("SIYI"), QStringLiteral("INFO"),
                             QStringLiteral("[EVENT] SIYI Link Status -> %1 (%2@%3 bps)")
                                 .arg(_siyiStatus == 2 ? "ONLINE" : (_siyiStatus == 1 ? "STANDBY" : "OFFLINE"))
                                 .arg(_siyiPort).arg(_siyiBaud),
                             _siyiStatus == 2 ? 6 : 4);
            _prevSiyiStatus = _siyiStatus;
        }
    }

    if (!portStr.isEmpty()) {
        if (!_availablePorts.contains(portStr)) {
            _availablePorts.append(portStr);
            emit availablePortsChanged();
        }
    }

    emit linksChanged();
    return true;
}

QVariantList CompanionLinksService::serialLinks() const
{
    QVariantList result;
    for (auto it = _indexedLinks.cbegin(); it != _indexedLinks.cend(); ++it) {
        auto entry = it.value();
        entry["fresh"] = _clock.elapsed() - _receivedAt.value(it.key()) <= 3500;
        result.append(entry);
    }
    return result;
}

void CompanionLinksService::sendLinksConfig(Vehicle* vehicle, int fcBaud, int siyiBaud, const QString& fcPort, const QString& siyiPort)
{
    if (!vehicle) {
        vehicle = MultiVehicleManager::instance()->activeVehicle();
    }

    SharedLinkInterfacePtr sharedLink;
    if (vehicle && vehicle->vehicleLinkManager()) {
        sharedLink = vehicle->vehicleLinkManager()->primaryLink().lock();
    }
    if (!sharedLink) {
        const auto links = LinkManager::instance()->links();
        for (const auto& l : links) {
            if (l && l->isConnected()) {
                sharedLink = l;
                break;
            }
        }
    }

    if (!sharedLink) {
        qWarning() << "[CompanionLinksService] Cannot send config: no active link found";
        return;
    }

    mavlink_message_t msg;
    // Send FC link
    mavlink_cc_serial_link_t fcLink{};
    fcLink.link_index = 0;
    fcLink.link_count = 2;
    fcLink.baudrate = static_cast<uint32_t>(fcBaud);
    fcLink.status = static_cast<uint8_t>(_fcStatus);
    strncpy(fcLink.name, "FC", sizeof(fcLink.name) - 1);
    QByteArray fcBytes = fcPort.toUtf8();
    strncpy(fcLink.port, fcBytes.constData(), sizeof(fcLink.port) - 1);

    mavlink_msg_cc_serial_link_encode_chan(
        MAVLinkProtocol::instance()->getSystemId(),
        MAVLinkProtocol::getComponentId(),
        sharedLink->mavlinkChannel(),
        &msg,
        &fcLink
    );

    if (vehicle) {
        vehicle->sendMessageOnLinkThreadSafe(sharedLink.get(), msg);
    } else {
        sharedLink->sendMessageThreadSafe(msg);
    }

    // Send SIYI link
    mavlink_cc_serial_link_t siyiLink{};
    siyiLink.link_index = 1;
    siyiLink.link_count = 2;
    siyiLink.baudrate = static_cast<uint32_t>(siyiBaud);
    siyiLink.status = static_cast<uint8_t>(_siyiStatus);
    strncpy(siyiLink.name, "SIYI", sizeof(siyiLink.name) - 1);
    QByteArray siyiBytes = siyiPort.toUtf8();
    strncpy(siyiLink.port, siyiBytes.constData(), sizeof(siyiLink.port) - 1);

    mavlink_msg_cc_serial_link_encode_chan(
        MAVLinkProtocol::instance()->getSystemId(),
        MAVLinkProtocol::getComponentId(),
        sharedLink->mavlinkChannel(),
        &msg,
        &siyiLink
    );

    if (vehicle) {
        vehicle->sendMessageOnLinkThreadSafe(sharedLink.get(), msg);
    } else {
        sharedLink->sendMessageThreadSafe(msg);
    }
}
