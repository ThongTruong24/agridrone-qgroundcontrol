#pragma once

#include <QtCore/QMetaObject>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QVariantList>
#include <QtQmlIntegration/QtQmlIntegration>
#include <array>

class Fact;
class Vehicle;

class AgriDroneController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    friend class AgriDroneControllerTest;

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool actuatorAvailable READ actuatorAvailable NOTIFY actuatorAvailableChanged)
    Q_PROPERTY(bool actuatorOn READ actuatorOn NOTIFY actuatorOnChanged)
    Q_PROPERTY(bool anyActuatorAvailable READ anyActuatorAvailable NOTIFY actuatorStatesChanged)
    Q_PROPERTY(QVariantList actuatorAvailableStates READ actuatorAvailableStates NOTIFY actuatorStatesChanged)
    Q_PROPERTY(QVariantList actuatorOnStates READ actuatorOnStates NOTIFY actuatorStatesChanged)

public:
    explicit AgriDroneController(QObject* parent = nullptr);

    bool enabled() const { return _enabled; }

    void setEnabled(bool enabled);

    bool actuatorAvailable() const { return _actuatorAvailableStates[0]; }

    bool actuatorOn() const { return _actuatorOnStates[0]; }

    bool anyActuatorAvailable() const;

    QVariantList actuatorAvailableStates() const;

    QVariantList actuatorOnStates() const;

    Q_INVOKABLE void setActuatorOn(bool on);
    Q_INVOKABLE void setActuatorOn(int actuatorIndex, bool on);

signals:
    void enabledChanged(bool enabled);
    void actuatorAvailableChanged(bool available);
    void actuatorOnChanged(bool on);
    void actuatorStatesChanged();

private:
    static constexpr int kActuatorCount = 4;

    void _setActiveVehicle(Vehicle* vehicle);
    void _updateActuatorFacts();
    void _setActuatorFact(int actuatorIndex, Fact* fact);
    void _updateFactState(int actuatorIndex);

    QPointer<Vehicle> _activeVehicle;
    std::array<QPointer<Fact>, kActuatorCount> _actuatorFacts;
    QMetaObject::Connection _parametersReadyConnection;
    QMetaObject::Connection _vehicleDestroyedConnection;
    std::array<QMetaObject::Connection, kActuatorCount> _factValueConnections;
    std::array<QMetaObject::Connection, kActuatorCount> _factDestroyedConnections;
    std::array<bool, kActuatorCount> _actuatorAvailableStates{};
    std::array<bool, kActuatorCount> _actuatorOnStates{};
    bool _enabled = false;
};
