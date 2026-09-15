#include "CustomFirmwarePlugin.h"

#include "CustomAutoPilotPlugin.h"
#include "Vehicle.h"
#include "THACOMAVLink.h"

AutoPilotPlugin* CustomFirmwarePlugin::autopilotPlugin(Vehicle* vehicle) const
{
    return new CustomAutoPilotPlugin(vehicle, vehicle);
}

QList<MAV_CMD> CustomFirmwarePlugin::supportedMissionCommands(QGCMAVLink::VehicleClass_t vehicleClass) const
{
    QList<MAV_CMD> supportedCommands = PX4FirmwarePlugin::supportedMissionCommands(vehicleClass);

    // Add THACO External XYZ command for all vehicle classes
    if (!supportedCommands.contains(MAV_CMD_THACO_EXTERNAL_XYZ_ENUM)) {
        supportedCommands.append(MAV_CMD_THACO_EXTERNAL_XYZ_ENUM);
    }

    return supportedCommands;
}

QString CustomFirmwarePlugin::missionCommandOverrides(QGCMAVLink::VehicleClass_t vehicleClass) const
{
    // THACO command is defined in the Generic base metadata (MavCmdInfoCommon.json).
    // For Generic vehicle class, return the PX4 Generic override (which may be empty).
    // For other vehicle classes, delegate to PX4 implementation.
    return PX4FirmwarePlugin::missionCommandOverrides(vehicleClass);
}
