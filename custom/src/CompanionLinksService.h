

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariantList>
#include <QtCore/QMap>
#include <QtCore/QElapsedTimer>

#include "ITelemetryHandler.h"
#include "Vehicle.h"

/**
 * @brief Serial link telemetry of every link the CC reports, by index and name (SOLID: Single Responsibility)
 */
class CompanionLinksService : public QObject, public ITelemetryHandler
{
    Q_OBJECT
public:
    explicit CompanionLinksService(QObject* parent = nullptr);
    ~CompanionLinksService() override = default;

    // ITelemetryHandler interface
    bool handleMavlinkMessage(const mavlink_message_t& message) override;
    void resetState() override;

    QVariantList serialLinks() const;

    // Telemetry getters
    bool hasLinksTelemetry() const { return _hasLinksTelemetry; }

    int linkStatusFlags() const { return _linkStatusFlags; }

    QString transportProtocol() const { return _transportProtocol; }

signals:
    void linksChanged();
    void logMessage(const QString& category, const QString& direction, const QString& message, int severity);

private:
    QElapsedTimer _clock;
    QMap<int, QVariantMap> _indexedLinks;
    QMap<int, qint64> _receivedAt;
    bool _hasLinksTelemetry = false;
    QString _transportProtocol{QStringLiteral("Serial / UART")};
    int _linkStatusFlags = 0;
    QMap<int, int> _prevStatus; // per link index: the status last logged
};
