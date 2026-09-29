#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QMetaObject>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QVariantMap>
#include <QtQmlIntegration/QtQmlIntegration>

#include "QGCMAVLink.h"

class Vehicle;
Q_MOC_INCLUDE("Vehicle.h")

class CcTelemetryController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

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

    Q_PROPERTY(bool vehicleAvailable READ vehicleAvailable NOTIFY vehicleAvailableChanged)
    Q_PROPERTY(int sourceSystemId READ sourceSystemId NOTIFY sourceChanged)
    Q_PROPERTY(int sourceComponentId READ sourceComponentId NOTIFY sourceChanged)
    Q_PROPERTY(int staleTimeoutMs READ staleTimeoutMs CONSTANT)

public:
    explicit CcTelemetryController(QObject* parent = nullptr);

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

    bool vehicleAvailable() const { return !_activeVehicle.isNull(); }

    int sourceSystemId() const { return _sourceSystemId; }

    int sourceComponentId() const { return _sourceComponentId; }

    int staleTimeoutMs() const { return kStaleTimeoutMs; }

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
    void vehicleAvailableChanged();
    void sourceChanged();

private slots:
    void _setActiveVehicle(Vehicle* vehicle);
    void _mavlinkMessageReceived(const mavlink_message_t& message);
    void _updateStaleStates();

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
    void _markReceived(MessageState& state, void (CcTelemetryController::*statusSignal)());
    void _updateStale(MessageState& state, void (CcTelemetryController::*statusSignal)());
    void _resetTelemetry();

    QPointer<Vehicle> _activeVehicle;
    QMetaObject::Connection _vehicleMessageConnection;
    QMetaObject::Connection _vehicleDestroyedConnection;
    QElapsedTimer _clock;
    QTimer _staleTimer;

    QVariantMap _links;
    QVariantMap _camera;
    QVariantMap _network;
    QVariantMap _vision;
    MessageState _linksState;
    MessageState _cameraState;
    MessageState _networkState;
    MessageState _visionState;
    int _sourceSystemId = -1;
    int _sourceComponentId = -1;
};
