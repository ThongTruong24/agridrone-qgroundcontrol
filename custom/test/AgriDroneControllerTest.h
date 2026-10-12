#pragma once

#include "UnitTest.h"

class AgriDroneControllerTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testEnabledToggle();
    void _testDefaultActuatorStates();
    void _testFactDrivenAvailability();
    void _testFactDrivenOnState();
    void _testSetActuatorOnGuardedWithoutVehicle();
};
