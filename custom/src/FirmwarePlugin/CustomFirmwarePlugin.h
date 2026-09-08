#pragma once

#include "PX4FirmwarePlugin.h"

class CustomFirmwarePlugin : public PX4FirmwarePlugin
{
    Q_OBJECT

public:
    AutoPilotPlugin* autopilotPlugin(Vehicle* vehicle) const final;
};
