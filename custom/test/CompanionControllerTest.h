#pragma once

#include "UnitTest.h"
#include "VehicleTestManualConnect.h"

class CompanionControllerTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testGeneratedMessageDecoding();
    void _testSourceFiltering();
    void _testStaleAndReset();
    void _testCompanionPageLoads();
    void _testMissionAndLogFiltering();
    void _testConfigResponses();
};

class CompanionVehicleLifecycleTest : public VehicleTestManualConnect
{
    Q_OBJECT

private slots:
    void _testDisconnectAndReconnectReset();
    void _testUartAckFilteringAndRetry();
    void _testQmlDraftAndSaveGuard();
};
