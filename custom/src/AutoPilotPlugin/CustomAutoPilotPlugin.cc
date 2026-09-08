#include "CustomAutoPilotPlugin.h"

#include "ActuatorComponent.h"
#include "AgriDroneComponent.h"
#include "MotorComponent.h"
#include "VehicleComponent.h"

CustomAutoPilotPlugin::CustomAutoPilotPlugin(Vehicle* vehicle, QObject* parent) : PX4AutoPilotPlugin(vehicle, parent) {}

const QVariantList& CustomAutoPilotPlugin::vehicleComponents()
{
    if (_agriDroneComponent) {
        return _customComponents;
    }

    const QVariantList& standardComponents = PX4AutoPilotPlugin::vehicleComponents();
    if (standardComponents.isEmpty() || !_vehicle) {
        return standardComponents;
    }

    _customComponents = standardComponents;
    _agriDroneComponent = new AgriDroneComponent(_vehicle, this, this);
    _agriDroneComponent->setupTriggerSignals();

    qsizetype insertIndex = _customComponents.size();
    for (qsizetype index = 0; index < _customComponents.size(); index++) {
        VehicleComponent* component =
            qobject_cast<VehicleComponent*>(qvariant_cast<QObject*>(_customComponents.at(index)));
        if (qobject_cast<ActuatorComponent*>(component) || qobject_cast<MotorComponent*>(component)) {
            insertIndex = index + 1;
            break;
        }
    }

    _customComponents.insert(insertIndex, QVariant::fromValue(static_cast<VehicleComponent*>(_agriDroneComponent)));
    return _customComponents;
}
