#pragma once

#include "VehicleComponent.h"

class AgriDroneComponent : public VehicleComponent
{
    Q_OBJECT

public:
    explicit AgriDroneComponent(Vehicle* vehicle, AutoPilotPlugin* autopilot, QObject* parent = nullptr);

    QStringList setupCompleteChangedTriggerList() const final { return QStringList(); }

    QString name() const final { return _name; }

    QString iconResource() const final { return QStringLiteral("/InstrumentValueIcons/drone.svg"); }

    bool requiresSetup() const final { return false; }

    bool setupComplete() const final { return true; }

    QUrl setupSource() const final;

    QUrl summaryQmlSource() const final { return QUrl(); }

private:
    const QString _name;
};
