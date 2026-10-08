

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
 * @brief Service responsible for FC and SIYI serial/telemetry link parameters (SOLID: Single Responsibility)
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

    int fcBaud() const { return _fcBaud; }

    int siyiBaud() const { return _siyiBaud; }

    QString fcPort() const { return _fcPort; }

    QString siyiPort() const { return _siyiPort; }

    int fcStatus() const { return _fcStatus; }

    int siyiStatus() const { return _siyiStatus; }

    float fcBitrateKbps() const { return _fcBitrateKbps; }

    float fcPacketDropRate() const { return _fcPacketDropRate; }

    float siyiBitrateKbps() const { return _siyiBitrateKbps; }

    float siyiPacketDropRate() const { return _siyiPacketDropRate; }

    quint32 fcBytesRx() const { return _fcBytesRx; }

    quint32 fcBytesTx() const { return _fcBytesTx; }

    quint32 siyiBytesRx() const { return _siyiBytesRx; }

    quint32 siyiBytesTx() const { return _siyiBytesTx; }

    int siyiLinkQuality() const { return _siyiLinkQuality; }

    int linkStatusFlags() const { return _linkStatusFlags; }

    float fcTxRate() const { return _fcTxRate; }

    float fcRxRate() const { return _fcRxRate; }

    float fcTxRateMax() const { return _fcTxRateMax; }

    float fcTxRateMulti() const { return _fcTxRateMulti; }

    float fcRxLoss() const { return _fcRxLoss; }

    quint32 fcTxErr() const { return _fcTxErr; }

    float siyiTxRate() const { return _siyiTxRate; }

    float siyiRxRate() const { return _siyiRxRate; }

    float siyiTxRateMax() const { return _siyiTxRateMax; }

    float siyiTxRateMulti() const { return _siyiTxRateMulti; }

    float siyiRxLoss() const { return _siyiRxLoss; }

    quint32 siyiTxErr() const { return _siyiTxErr; }

    QString transportProtocol() const { return _transportProtocol; }

    QStringList availablePorts() const {
        QStringList ports = _availablePorts;
        if (!_fcPort.isEmpty() && !ports.contains(_fcPort)) {
            ports.append(_fcPort);
        }
        if (!_siyiPort.isEmpty() && !ports.contains(_siyiPort)) {
            ports.append(_siyiPort);
        }
        return ports;
    }

    void sendLinksConfig(Vehicle* vehicle, int fcBaud, int siyiBaud, const QString& fcPort, const QString& siyiPort);

signals:
    void linksChanged();
    void availablePortsChanged();
    void logMessage(const QString& category, const QString& direction, const QString& message, int severity);

private:
    QElapsedTimer _clock;
    QMap<int, QVariantMap> _indexedLinks;
    QMap<int, qint64> _receivedAt;
    bool _hasLinksTelemetry = false;
    int _fcBaud = 0;
    int _siyiBaud = 0;
    QString _fcPort;
    QString _siyiPort;
    QString _transportProtocol{QStringLiteral("Serial / UART")};
    int _fcStatus = 0;
    int _siyiStatus = 0;
    float _fcBitrateKbps = 0.0f;
    float _fcPacketDropRate = 0.0f;
    float _siyiBitrateKbps = 0.0f;
    float _siyiPacketDropRate = 0.0f;
    quint32 _fcBytesRx = 0;
    quint32 _fcBytesTx = 0;
    quint32 _siyiBytesRx = 0;
    quint32 _siyiBytesTx = 0;
    int _siyiLinkQuality = 0;
    int _linkStatusFlags = 0;

    float _fcTxRate = 0.0f;
    float _fcRxRate = 0.0f;
    float _fcTxRateMax = 0.0f;
    float _fcTxRateMulti = 0.0f;
    float _fcRxLoss = 0.0f;
    quint32 _fcTxErr = 0;

    float _siyiTxRate = 0.0f;
    float _siyiRxRate = 0.0f;
    float _siyiTxRateMax = 0.0f;
    float _siyiTxRateMulti = 0.0f;
    float _siyiRxLoss = 0.0f;
    quint32 _siyiTxErr = 0;

    QStringList _availablePorts;
    int _prevFcStatus = -1;
    int _prevSiyiStatus = -1;
};
