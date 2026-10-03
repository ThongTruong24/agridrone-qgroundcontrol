#include "CompanionLinksService.h"
#include "MAVLinkProtocol.h"
#include "MultiVehicleManager.h"
#include "LinkManager.h"
#include "VehicleLinkManager.h"
#include <QtCore/QDebug>

CompanionLinksService::CompanionLinksService(QObject* parent)
    : QObject(parent)
{
    resetState();
}

void CompanionLinksService::resetState()
{
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
    if (message.msgid != 42010) { // CC_TELEMETRY_LINKS
        return false;
    }

    // Accept only from Companion Computer component ID (191) or broadcast (0)
    if (message.compid != 191 && message.compid != 0) {
        return false;
    }

    mavlink_cc_telemetry_links_t lnk;
    mavlink_msg_cc_telemetry_links_decode(&message, &lnk);

    _hasLinksTelemetry = true;
    _fcBaud = lnk.fc_baudrate;
    _siyiBaud = lnk.siyi_baudrate;
    _fcPort = QString::fromUtf8(lnk.fc_port, qstrnlen(lnk.fc_port, sizeof(lnk.fc_port)));
    _siyiPort = QString::fromUtf8(lnk.siyi_port, qstrnlen(lnk.siyi_port, sizeof(lnk.siyi_port)));
    _fcStatus = lnk.fc_status;
    _siyiStatus = lnk.siyi_status;

    _fcTxRate = lnk.fc_tx_rate;
    _fcRxRate = lnk.fc_rx_rate;
    _fcTxRateMax = lnk.fc_tx_rate_max;
    _fcTxRateMulti = lnk.fc_tx_rate_multi;
    _fcRxLoss = lnk.fc_rx_loss;
    _fcTxErr = lnk.fc_tx_err;
    _fcBytesRx = lnk.fc_bytes_rx;
    _fcBytesTx = lnk.fc_bytes_tx;
    _fcBitrateKbps = (_fcRxRate * 8.0f) / 1000.0f;
    _fcPacketDropRate = _fcRxLoss;

    _siyiTxRate = lnk.siyi_tx_rate;
    _siyiRxRate = lnk.siyi_rx_rate;
    _siyiTxRateMax = lnk.siyi_tx_rate_max;
    _siyiTxRateMulti = lnk.siyi_tx_rate_multi;
    _siyiRxLoss = lnk.siyi_rx_loss;
    _siyiTxErr = lnk.siyi_tx_err;
    _siyiBytesRx = lnk.siyi_bytes_rx;
    _siyiBytesTx = lnk.siyi_bytes_tx;
    _siyiBitrateKbps = (_siyiRxRate * 8.0f) / 1000.0f;
    _siyiPacketDropRate = _siyiRxLoss;
    _siyiLinkQuality = (_siyiRxLoss < 100.0f) ? static_cast<int>(100.0f - _siyiRxLoss) : 0;

    switch (lnk.transport_type) {
    case 1: _transportProtocol = QStringLiteral("Serial / UART"); break;
    case 2: _transportProtocol = QStringLiteral("UDP"); break;
    case 3: _transportProtocol = QStringLiteral("TCP"); break;
    default: _transportProtocol = QStringLiteral("Serial / UART"); break;
    }

    // Available serial ports parsing
    QString portsRaw = QString::fromUtf8(lnk.available_ports, qstrnlen(lnk.available_ports, sizeof(lnk.available_ports))).trimmed();
    if (!portsRaw.isEmpty()) {
        QStringList portTokens = portsRaw.split(',', Qt::SkipEmptyParts);
        QStringList fullPaths;
        for (const QString& p : portTokens) {
            QString trimmed = p.trimmed();
            if (!trimmed.startsWith("/dev/")) {
                fullPaths.append("/dev/" + trimmed);
            } else {
                fullPaths.append(trimmed);
            }
        }
        if (!fullPaths.isEmpty() && fullPaths != _availablePorts) {
            _availablePorts = fullPaths;
            emit availablePortsChanged();
        }
    }

    if (_fcStatus != _prevFcStatus) {
        emit logMessage(QStringLiteral("FC"), QStringLiteral("INFO"),
                         QStringLiteral("[EVENT] FC Link Status -> %1 (%2@%3 bps)")
                             .arg(_fcStatus == 2 ? "ONLINE" : (_fcStatus == 1 ? "STANDBY" : "OFFLINE"))
                             .arg(_fcPort).arg(_fcBaud),
                         _fcStatus == 2 ? 6 : 4);
        _prevFcStatus = _fcStatus;
    }

    if (_siyiStatus != _prevSiyiStatus) {
        emit logMessage(QStringLiteral("SIYI"), QStringLiteral("INFO"),
                         QStringLiteral("[EVENT] SIYI Link Status -> %1 (%2@%3 bps)")
                             .arg(_siyiStatus == 2 ? "ONLINE" : (_siyiStatus == 1 ? "STANDBY" : "OFFLINE"))
                             .arg(_siyiPort).arg(_siyiBaud),
                         _siyiStatus == 2 ? 6 : 4);
        _prevSiyiStatus = _siyiStatus;
    }

    emit linksChanged();
    return true;
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
    mavlink_cc_telemetry_links_t l{};
    l.fc_baudrate = static_cast<uint32_t>(fcBaud);
    l.siyi_baudrate = static_cast<uint32_t>(siyiBaud);
    l.fc_bytes_rx = _fcBytesRx;
    l.fc_bytes_tx = _fcBytesTx;
    l.fc_tx_rate = _fcTxRate;
    l.fc_rx_rate = _fcRxRate;
    l.fc_tx_rate_max = _fcTxRateMax;
    l.fc_tx_rate_multi = _fcTxRateMulti;
    l.fc_rx_loss = _fcRxLoss;
    l.fc_tx_err = _fcTxErr;
    l.fc_status = static_cast<uint8_t>(_fcStatus);
    l.siyi_status = static_cast<uint8_t>(_siyiStatus);
    l.siyi_bytes_rx = _siyiBytesRx;
    l.siyi_bytes_tx = _siyiBytesTx;
    l.siyi_tx_rate = _siyiTxRate;
    l.siyi_rx_rate = _siyiRxRate;
    l.siyi_tx_rate_max = _siyiTxRateMax;
    l.siyi_tx_rate_multi = _siyiTxRateMulti;
    l.siyi_rx_loss = _siyiRxLoss;
    l.siyi_tx_err = _siyiTxErr;
    l.transport_type = 1;

    QByteArray fcBytes = fcPort.toUtf8();
    strncpy(l.fc_port, fcBytes.constData(), sizeof(l.fc_port) - 1);
    QByteArray siyiBytes = siyiPort.toUtf8();
    strncpy(l.siyi_port, siyiBytes.constData(), sizeof(l.siyi_port) - 1);

    mavlink_msg_cc_telemetry_links_encode_chan(
        MAVLinkProtocol::instance()->getSystemId(),
        MAVLinkProtocol::getComponentId(),
        sharedLink->mavlinkChannel(),
        &msg,
        &l
    );

    if (vehicle) {
        vehicle->sendMessageOnLinkThreadSafe(sharedLink.get(), msg);
    } else {
        sharedLink->sendMessageThreadSafe(msg);
    }
}
