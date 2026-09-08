#include "CustomFirmwarePluginFactory.h"

#include "CustomFirmwarePlugin.h"

CustomFirmwarePluginFactory CustomFirmwarePluginFactoryImp;

QList<QGCMAVLink::FirmwareClass_t> CustomFirmwarePluginFactory::supportedFirmwareClasses() const
{
    return {QGCMAVLink::FirmwareClassPX4};
}

FirmwarePlugin* CustomFirmwarePluginFactory::firmwarePluginForAutopilot(MAV_AUTOPILOT autopilotType,
                                                                        MAV_TYPE /*vehicleType*/)
{
    if (autopilotType == MAV_AUTOPILOT_PX4) {
        if (!_pluginInstance) {
            _pluginInstance = new CustomFirmwarePlugin;
        }
        return _pluginInstance;
    }

    return nullptr;
}
