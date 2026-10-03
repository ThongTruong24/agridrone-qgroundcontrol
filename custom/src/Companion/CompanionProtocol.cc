#include "CompanionProtocol.h"

#include "LinkManager.h"
#include "MAVLinkProtocol.h"
#include "Vehicle.h"
#include "VehicleLinkManager.h"

namespace Companion {

SharedLinkInterfacePtr resolveLink(Vehicle* vehicle)
{
    if (vehicle && vehicle->vehicleLinkManager()) {
        if (SharedLinkInterfacePtr link = vehicle->vehicleLinkManager()->primaryLink().lock()) {
            return link;
        }
    }
    for (const SharedLinkInterfacePtr& link : LinkManager::instance()->links()) {
        if (link && link->isConnected()) {
            return link;
        }
    }
    return {};
}

bool sendMessage(Vehicle* vehicle, const SharedLinkInterfacePtr& link, const mavlink_message_t& msg)
{
    if (!link) {
        return false;
    }
    if (vehicle) {
        return vehicle->sendMessageOnLinkThreadSafe(link.get(), msg);
    }
    mavlink_message_t copy = msg;
    link->sendMessageThreadSafe(copy);
    return true;
}

bool sendCommand(Vehicle* vehicle, uint8_t targetSystem, MAV_CMD command, float param1, float param2)
{
    const SharedLinkInterfacePtr link = resolveLink(vehicle);
    if (!link) {
        return false;
    }
    mavlink_command_long_t cmd{};
    cmd.target_system    = targetSystem;
    cmd.target_component = kCompId;
    cmd.command          = command;
    cmd.param1           = param1;
    cmd.param2           = param2;

    mavlink_message_t msg;
    mavlink_msg_command_long_encode_chan(MAVLinkProtocol::instance()->getSystemId(),
                                         MAVLinkProtocol::getComponentId(),
                                         link->mavlinkChannel(), &msg, &cmd);
    return sendMessage(vehicle, link, msg);
}

} // namespace Companion
