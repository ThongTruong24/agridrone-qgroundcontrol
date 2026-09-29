#pragma once

#include "UnitTest.h"
#include "VehicleTestManualConnect.h"

class CcTelemetryControllerTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testGeneratedMessageDecoding();
    void _testSourceFiltering();
    void _testStaleAndReset();
};

class CcTelemetryVehicleLifecycleTest : public VehicleTestManualConnect
{
    Q_OBJECT

private slots:
    void _testDisconnectAndReconnectReset();
};
