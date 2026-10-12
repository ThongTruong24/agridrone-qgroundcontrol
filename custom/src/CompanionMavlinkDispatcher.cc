#include "CompanionMavlinkDispatcher.h"

CompanionMavlinkDispatcher::CompanionMavlinkDispatcher(QObject* parent)
    : QObject(parent)
{
}

void CompanionMavlinkDispatcher::registerHandler(ITelemetryHandler* handler)
{
    if (handler && !_handlers.contains(handler)) {
        _handlers.append(handler);
    }
}

void CompanionMavlinkDispatcher::unregisterHandler(ITelemetryHandler* handler)
{
    _handlers.removeAll(handler);
}

bool CompanionMavlinkDispatcher::dispatchMessage(const mavlink_message_t& message)
{
    bool handled = false;
    for (ITelemetryHandler* handler : _handlers) {
        if (handler && handler->handleMavlinkMessage(message)) {
            handled = true;
        }
    }
    return handled;
}

void CompanionMavlinkDispatcher::resetAllHandlers()
{
    for (ITelemetryHandler* handler : _handlers) {
        if (handler) {
            handler->resetState();
        }
    }
}
