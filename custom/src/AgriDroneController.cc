#include "AgriDroneController.h"

#include <algorithm>

#include "CustomFirmwarePlugin.h"
#include "Fact.h"
#include "MultiVehicleManager.h"
#include "ParameterManager.h"
#include "Vehicle.h"

namespace {
const std::array<QString, 4> kActuatorParameterNames{
    QStringLiteral("THACO_A1_DEF"),
    QStringLiteral("THACO_A2_DEF"),
    QStringLiteral("THACO_A3_DEF"),
    QStringLiteral("THACO_A4_DEF"),
};
constexpr int kActuatorOnValue = 255;
constexpr int kActuatorOffValue = 0;
}  // namespace

AgriDroneController::AgriDroneController(QObject* parent) : QObject(parent)
{
    MultiVehicleManager* const multiVehicleManager = MultiVehicleManager::instance();
    (void) connect(multiVehicleManager, &MultiVehicleManager::activeVehicleChanged, this,
                   &AgriDroneController::_setActiveVehicle);
    _setActiveVehicle(multiVehicleManager->activeVehicle());
}

void AgriDroneController::setEnabled(bool enabled)
{
    if (_enabled != enabled) {
        _enabled = enabled;
        emit enabledChanged(enabled);
    }
}

bool AgriDroneController::anyActuatorAvailable() const
{
    return std::any_of(_actuatorAvailableStates.cbegin(), _actuatorAvailableStates.cend(),
                       [](bool available) { return available; });
}

QVariantList AgriDroneController::actuatorAvailableStates() const
{
    QVariantList states;
    states.reserve(kActuatorCount);
    for (const bool state : _actuatorAvailableStates) {
        states.append(state);
    }
    return states;
}

QVariantList AgriDroneController::actuatorOnStates() const
{
    QVariantList states;
    states.reserve(kActuatorCount);
    for (const bool state : _actuatorOnStates) {
        states.append(state);
    }
    return states;
}

void AgriDroneController::setActuatorOn(bool on)
{
    setActuatorOn(0, on);
}

void AgriDroneController::setActuatorOn(int actuatorIndex, bool on)
{
    if (actuatorIndex < 0 || actuatorIndex >= kActuatorCount || !_enabled || !_activeVehicle ||
        !_actuatorAvailableStates[actuatorIndex] || !_actuatorFacts[actuatorIndex]) {
        return;
    }

    _actuatorFacts[actuatorIndex]->setRawValue(on ? kActuatorOnValue : kActuatorOffValue);
}

void AgriDroneController::_setActiveVehicle(Vehicle* vehicle)
{
    if (_activeVehicle == vehicle) {
        _updateActuatorFacts();
        return;
    }

    QObject::disconnect(_parametersReadyConnection);
    QObject::disconnect(_vehicleDestroyedConnection);
    _activeVehicle = vehicle;

    if (_activeVehicle) {
        _vehicleDestroyedConnection = connect(_activeVehicle, &QObject::destroyed, this, [this]() {
            _activeVehicle = nullptr;
            _updateActuatorFacts();
        });

        if (qobject_cast<CustomFirmwarePlugin*>(_activeVehicle->firmwarePlugin())) {
            ParameterManager* const parameterManager = _activeVehicle->parameterManager();
            _parametersReadyConnection = connect(parameterManager, &ParameterManager::parametersReadyChanged, this,
                                                 [this](bool) { _updateActuatorFacts(); });
        }
    }

    _updateActuatorFacts();
}

void AgriDroneController::_updateActuatorFacts()
{
    std::array<Fact*, kActuatorCount> actuatorFacts{};

    if (_activeVehicle && qobject_cast<CustomFirmwarePlugin*>(_activeVehicle->firmwarePlugin())) {
        ParameterManager* const parameterManager = _activeVehicle->parameterManager();
        if (parameterManager->parametersReady()) {
            for (int actuatorIndex = 0; actuatorIndex < kActuatorCount; ++actuatorIndex) {
                const QString& parameterName = kActuatorParameterNames[actuatorIndex];
                if (parameterManager->parameterExists(ParameterManager::defaultComponentId, parameterName)) {
                    actuatorFacts[actuatorIndex] =
                        parameterManager->getParameter(ParameterManager::defaultComponentId, parameterName);
                }
            }
        }
    }

    for (int actuatorIndex = 0; actuatorIndex < kActuatorCount; ++actuatorIndex) {
        _setActuatorFact(actuatorIndex, actuatorFacts[actuatorIndex]);
    }
}

void AgriDroneController::_setActuatorFact(int actuatorIndex, Fact* fact)
{
    if (_actuatorFacts[actuatorIndex] == fact) {
        _updateFactState(actuatorIndex);
        return;
    }

    QObject::disconnect(_factValueConnections[actuatorIndex]);
    QObject::disconnect(_factDestroyedConnections[actuatorIndex]);
    _actuatorFacts[actuatorIndex] = fact;

    if (_actuatorFacts[actuatorIndex]) {
        _factValueConnections[actuatorIndex] =
            connect(_actuatorFacts[actuatorIndex], &Fact::rawValueChanged, this,
                    [this, actuatorIndex](const QVariant&) { _updateFactState(actuatorIndex); });
        _factDestroyedConnections[actuatorIndex] =
            connect(_actuatorFacts[actuatorIndex], &QObject::destroyed, this, [this, actuatorIndex]() {
                _actuatorFacts[actuatorIndex] = nullptr;
                _updateFactState(actuatorIndex);
            });
    }

    _updateFactState(actuatorIndex);
}

void AgriDroneController::_updateFactState(int actuatorIndex)
{
    const bool actuatorAvailable = !_actuatorFacts[actuatorIndex].isNull();
    const bool actuatorOn =
        actuatorAvailable && (_actuatorFacts[actuatorIndex]->rawValue().toInt() == kActuatorOnValue);
    bool stateChanged = false;

    if (_actuatorAvailableStates[actuatorIndex] != actuatorAvailable) {
        _actuatorAvailableStates[actuatorIndex] = actuatorAvailable;
        stateChanged = true;
        if (actuatorIndex == 0) {
            emit actuatorAvailableChanged(actuatorAvailable);
        }
    }

    if (_actuatorOnStates[actuatorIndex] != actuatorOn) {
        _actuatorOnStates[actuatorIndex] = actuatorOn;
        stateChanged = true;
        if (actuatorIndex == 0) {
            emit actuatorOnChanged(actuatorOn);
        }
    }

    if (stateChanged) {
        emit actuatorStatesChanged();
    }
}
