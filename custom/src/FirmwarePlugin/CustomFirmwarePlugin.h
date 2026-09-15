#pragma once

#include "PX4FirmwarePlugin.h"

class CustomFirmwarePlugin : public PX4FirmwarePlugin
{
    Q_OBJECT

public:
    AutoPilotPlugin* autopilotPlugin(Vehicle* vehicle) const final;

    QList<MAV_CMD> supportedMissionCommands(QGCMAVLink::VehicleClass_t vehicleClass) const override;
    QString missionCommandOverrides(QGCMAVLink::VehicleClass_t vehicleClass) const override;
};
