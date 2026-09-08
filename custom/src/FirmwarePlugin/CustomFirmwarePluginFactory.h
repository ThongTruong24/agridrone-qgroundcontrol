#pragma once

#include "FirmwarePluginFactory.h"

class CustomFirmwarePlugin;

class CustomFirmwarePluginFactory : public FirmwarePluginFactory
{
    Q_OBJECT

public:
    QList<QGCMAVLink::FirmwareClass_t> supportedFirmwareClasses() const final;
    FirmwarePlugin* firmwarePluginForAutopilot(MAV_AUTOPILOT autopilotType, MAV_TYPE vehicleType) final;

private:
    CustomFirmwarePlugin* _pluginInstance = nullptr;
};

extern CustomFirmwarePluginFactory CustomFirmwarePluginFactoryImp;
