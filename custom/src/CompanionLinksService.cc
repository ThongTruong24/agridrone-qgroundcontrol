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
    _transportProtocol = QStringLiteral("Serial / UART");
    _linkStatusFlags = 0;
    _prevStatus.clear();

    emit linksChanged();
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
            _prevStatus.remove(it.key());
            it = _indexedLinks.erase(it);
        } else {
            ++it;
        }
    }

    if (_prevStatus.value(lnk.link_index, -1) != lnk.status) {
        emit logMessage(linkName, QStringLiteral("INFO"),
                        QStringLiteral("[EVENT] %1 link -> %2 (%3@%4 bps)")
                            .arg(linkName, lnk.status == 2 ? QStringLiteral("ONLINE") : lnk.status == 1 ? QStringLiteral("STANDBY")
                                                                                         : QStringLiteral("OFFLINE"),
                                 portStr, QString::number(lnk.baudrate)),
                        lnk.status == 2 ? 6 : 4);
        _prevStatus[lnk.link_index] = lnk.status;
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

