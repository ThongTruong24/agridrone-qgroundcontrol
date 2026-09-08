#pragma once

#include "PX4AutoPilotPlugin.h"

class AgriDroneComponent;

class CustomAutoPilotPlugin : public PX4AutoPilotPlugin
{
    Q_OBJECT

public:
    explicit CustomAutoPilotPlugin(Vehicle* vehicle, QObject* parent = nullptr);

    const QVariantList& vehicleComponents() final;

private:
    QVariantList _customComponents;
    AgriDroneComponent* _agriDroneComponent = nullptr;
};
