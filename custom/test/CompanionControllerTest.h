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
    void _testToolbarPopupsLoad();
};

class CompanionVehicleLifecycleTest : public VehicleTestManualConnect
{
    Q_OBJECT

private slots:
    void _testDisconnectAndReconnectReset();
    void _testStreamOnUrlAckBeforeEnableAndVehicleCancel();
    void _testStreamOptionsFromCapabilityParams();
    void _testApplyProgressFromCompanionStages();
    void _testLinkApplyGapConfirmedByTelemetry();
    void _testHotspotApplyGapConfirmedByTelemetry();
    void _testSetWithoutAnyAckReportsNoResponse();
    void _testSecretNeverAppearsInOperationSteps();
    void _testStreamOnSendsUrlEnablePublishInOrder();
    void _testStreamOffSendsOnlyEnableZero();
    void _testStreamOnSkipsWhatTelemetryShowsDone();
    void _testStreamOnRefusesInvalidAddress();
    void _testStreamReasonTable();
    void _testLinkPortAndBaudApplyAndConflictGuard();
    void _testHotspotApplyAllowedWhenApOff();
    void _testVisionConfidenceAndAiBranch();
};
