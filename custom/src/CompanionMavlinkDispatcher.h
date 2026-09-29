#pragma once

#include <QtCore/QObject>
#include <QtCore/QList>
#include "ITelemetryHandler.h"

/**
 * @brief Dispatcher for incoming MAVLink messages to decoupled handlers (SOLID: Open/Closed & Dependency Inversion)
 */
class CompanionMavlinkDispatcher : public QObject {
    Q_OBJECT
public:
    explicit CompanionMavlinkDispatcher(QObject* parent = nullptr);
    ~CompanionMavlinkDispatcher() override = default;

    void registerHandler(ITelemetryHandler* handler);
    void unregisterHandler(ITelemetryHandler* handler);

    bool dispatchMessage(const mavlink_message_t& message);
    void resetAllHandlers();

private:
    QList<ITelemetryHandler*> _handlers;
};
