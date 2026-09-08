#include "CustomFirmwarePlugin.h"

#include "CustomAutoPilotPlugin.h"
#include "Vehicle.h"

AutoPilotPlugin* CustomFirmwarePlugin::autopilotPlugin(Vehicle* vehicle) const
{
    return new CustomAutoPilotPlugin(vehicle, vehicle);
}
