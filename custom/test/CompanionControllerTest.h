#pragma once

#include "UnitTest.h"
#include "VehicleTestManualConnect.h"

class CompanionControllerTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testToolbarLegacyAndIndependentStreams();
    void _testGeneratedMessageDecoding();
    void _testSourceFiltering();
    void _testStaleAndReset();
    void _testCompanionPageLoads();
    void _testToolbarPopupsLoad();
    void _testMissionAndLogFiltering();
};

class CompanionVehicleLifecycleTest : public VehicleTestManualConnect
{
    Q_OBJECT

private slots:
    void _testDisconnectAndReconnectReset();
    void _testUartAckFilteringAndRetry();
    void _testConnectUrlAckBeforeEnableAndVehicleCancel();
    void _testQmlDraftAndSaveGuard();
    void _testStreamOptionsFromCapabilityParams();
};
