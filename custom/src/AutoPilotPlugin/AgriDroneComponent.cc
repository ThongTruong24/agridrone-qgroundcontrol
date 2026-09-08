#include "AgriDroneComponent.h"

AgriDroneComponent::AgriDroneComponent(Vehicle* vehicle, AutoPilotPlugin* autopilot, QObject* parent)
    : VehicleComponent(vehicle, autopilot, AutoPilotPlugin::UnknownVehicleComponent, parent), _name(tr("AgriDrone"))
{}

QUrl AgriDroneComponent::setupSource() const
{
    return QUrl::fromUserInput(QStringLiteral("qrc:/qml/Custom/AgriDrone/AgriDroneSettings.qml"));
}
