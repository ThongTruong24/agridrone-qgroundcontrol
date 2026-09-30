#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QMetaObject>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtQmlIntegration/QtQmlIntegration>

#include "QGCMAVLink.h"

class Vehicle;
Q_MOC_INCLUDE("Vehicle.h")

class CompanionController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    friend class CompanionVehicleLifecycleTest;

    Q_PROPERTY(QVariantMap ccTelemetryLinks READ ccTelemetryLinks NOTIFY linksChanged)
    Q_PROPERTY(QVariantMap ccTelemetryCamera READ ccTelemetryCamera NOTIFY cameraChanged)
    Q_PROPERTY(QVariantMap ccTelemetryNetwork READ ccTelemetryNetwork NOTIFY networkChanged)
    Q_PROPERTY(QVariantMap ccTelemetryVision READ ccTelemetryVision NOTIFY visionChanged)

    Q_PROPERTY(bool linksReceived READ linksReceived NOTIFY linksStatusChanged)
    Q_PROPERTY(bool linksStale READ linksStale NOTIFY linksStatusChanged)
    Q_PROPERTY(bool cameraReceived READ cameraReceived NOTIFY cameraStatusChanged)
    Q_PROPERTY(bool cameraStale READ cameraStale NOTIFY cameraStatusChanged)
    Q_PROPERTY(bool networkReceived READ networkReceived NOTIFY networkStatusChanged)
    Q_PROPERTY(bool networkStale READ networkStale NOTIFY networkStatusChanged)
    Q_PROPERTY(bool visionReceived READ visionReceived NOTIFY visionStatusChanged)
    Q_PROPERTY(bool visionStale READ visionStale NOTIFY visionStatusChanged)
    Q_PROPERTY(bool missionReceived READ missionReceived NOTIFY missionStatusChanged)
    Q_PROPERTY(bool missionStale READ missionStale NOTIFY missionStatusChanged)
    Q_PROPERTY(quint32 lastTriggerId READ lastTriggerId NOTIFY missionChanged)
    Q_PROPERTY(quint32 lastTriggerTimeBootMs READ lastTriggerTimeBootMs NOTIFY missionChanged)

    Q_PROPERTY(bool vehicleAvailable READ vehicleAvailable NOTIFY vehicleAvailableChanged)
    Q_PROPERTY(int sourceSystemId READ sourceSystemId NOTIFY sourceChanged)
    Q_PROPERTY(int sourceComponentId READ sourceComponentId NOTIFY sourceChanged)
    Q_PROPERTY(int staleTimeoutMs READ staleTimeoutMs CONSTANT)
    Q_PROPERTY(QString configStatus READ configStatus NOTIFY configStatusChanged)
    Q_PROPERTY(QString configMessage READ configMessage NOTIFY configStatusChanged)
    Q_PROPERTY(QStringList availablePorts READ availablePorts NOTIFY linksChanged)
    Q_PROPERTY(int vehicleEpoch READ vehicleEpoch NOTIFY vehicleEpochChanged)

public:
    explicit CompanionController(QObject* parent = nullptr);

    QVariantMap ccTelemetryLinks() const { return _links; }

    QVariantMap ccTelemetryCamera() const { return _camera; }

    QVariantMap ccTelemetryNetwork() const { return _network; }

    QVariantMap ccTelemetryVision() const { return _vision; }

    bool linksReceived() const { return _linksState.received; }

    bool linksStale() const { return _linksState.stale; }

    bool cameraReceived() const { return _cameraState.received; }

    bool cameraStale() const { return _cameraState.stale; }

    bool networkReceived() const { return _networkState.received; }

    bool networkStale() const { return _networkState.stale; }

    bool visionReceived() const { return _visionState.received; }

    bool visionStale() const { return _visionState.stale; }

    bool missionReceived() const { return _missionState.received; }

    bool missionStale() const { return _missionState.stale; }

    quint32 lastTriggerId() const { return _lastTriggerId; }

    quint32 lastTriggerTimeBootMs() const { return _lastTriggerTimeBootMs; }

    bool vehicleAvailable() const { return !_activeVehicle.isNull(); }

    int sourceSystemId() const { return _sourceSystemId; }

    int sourceComponentId() const { return _sourceComponentId; }

    int staleTimeoutMs() const { return kStaleTimeoutMs; }

    QString configStatus() const { return _configStatus; }

    QString configMessage() const { return _configMessage; }

    QStringList availablePorts() const;

    int vehicleEpoch() const { return _vehicleEpoch; }

    Q_INVOKABLE void applyLinksConfig(const QString& fcPort, int fcBaud, const QString& siyiPort, int siyiBaud);
    Q_INVOKABLE void saveLinksConfig();
    Q_INVOKABLE QVariantList getLogHistory(const QString& category = QString()) const;
    Q_INVOKABLE void clearLogHistory(const QString& category = QString());
    Q_INVOKABLE bool logMatchesFilter(const QString& filter, const QString& category, const QString& message) const;

#ifdef QGC_UNITTEST_BUILD
    void processMessageForTest(const mavlink_message_t& message);
    void forceStaleForTest();
    void resetForTest();
#endif

signals:
    void linksChanged();
    void cameraChanged();
    void networkChanged();
    void visionChanged();
    void linksStatusChanged();
    void cameraStatusChanged();
    void networkStatusChanged();
    void visionStatusChanged();
    void missionChanged();
    void missionStatusChanged();
    void vehicleAvailableChanged();
    void vehicleEpochChanged();
    void sourceChanged();
    void configStatusChanged();
    void commandAckReceived(int command, int result);
    void mavlinkLogMessage(const QString& category, const QString& direction, const QString& message, int severity);

private slots:
    void _setActiveVehicle(Vehicle* vehicle);
    void _mavlinkMessageReceived(const mavlink_message_t& message);
    void _updateStaleStates();
    void _configTimedOut();
    void _confirmationTimedOut();

private:
    struct MessageState
    {
        bool received = false;
        bool stale = false;
        qint64 lastReceivedMs = 0;
    };

    static constexpr int kStaleTimeoutMs = 5000;
    static constexpr int kStaleCheckIntervalMs = 1000;

    bool _acceptSource(const mavlink_message_t& message, bool requireActiveVehicle);
    void _processMessage(const mavlink_message_t& message);
    void _markReceived(MessageState& state, void (CompanionController::*statusSignal)());
    void _updateStale(MessageState& state, void (CompanionController::*statusSignal)());
    void _resetTelemetry();
    bool _sendCommand(quint16 command);
    void _setConfigStatus(const QString& status, const QString& message);
    void _processCommandAck(const mavlink_message_t& message);
    void _checkTelemetryConfirmation();

    QPointer<Vehicle> _activeVehicle;
    QMetaObject::Connection _vehicleMessageConnection;
    QMetaObject::Connection _vehicleDestroyedConnection;
    QElapsedTimer _clock;
    QTimer _staleTimer;
    QTimer _configTimer;
    QTimer _confirmationTimer;

    QVariantMap _links;
    QVariantMap _camera;
    QVariantMap _network;
    QVariantMap _vision;
    MessageState _linksState;
    MessageState _cameraState;
    MessageState _networkState;
    MessageState _visionState;
    MessageState _missionState;
    quint32 _lastTriggerId = 0;
    quint32 _lastTriggerTimeBootMs = 0;
    int _vehicleEpoch = 0;
    int _sourceSystemId = -1;
    int _sourceComponentId = -1;
    quint16 _pendingCommand = 0;
    bool _waitingTelemetry = false;
    QString _expectedFcPort;
    QString _expectedSiyiPort;
    int _expectedFcBaud = 0;
    int _expectedSiyiBaud = 0;
    QString _configStatus = QStringLiteral("Idle");
    QString _configMessage;
    class CompanionLogService* _logService = nullptr;
};
