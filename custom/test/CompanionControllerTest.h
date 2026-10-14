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
    void _testLinkApplyWithoutVehicle();
    void _testApplyNotResentWhenCompanionSilent();
    void _testFirstCompanionLocksWithoutVehicle();
    void _testFailedSendEndsOperationAtOnce();
    void _testCompanionAckRestartsWatchdog();
    void _testLockFromCompanionBitWithoutVehicle();
    void _testRefusalShowsCompanionReason();
    void _testApplyWaitsThroughHeartbeatLossThenReadsBack();
    void _testApplyWaitReadbackMismatchFails();
    void _testCancelCompanionWait();
    void _testNonApplyOpEndsOnHeartbeatLoss();
    void _testApplyLeavesUnsavedUntilSave();
    void _testThirdLinkIsConfigurable();
    void _testStreamModeWithoutVehicle();
    void _testStageRollbackWithoutVehicle();
    void _testTelemetryTimeoutKeepsApplyWait();
    void _testReadbackSkipsSecret();
    void _testLinkApplyWaitsThroughHeartbeatLossThenReadsBack();
    void _testApplySilenceAfterInProgressWaitsForTelemetry();
    void _testAcceptedApplyReadbackConfirms();
    void _testApplyWithoutReceiptReadbackIsUnconfirmed();
    void _testFailedLinkApplyKeepsEarlierUnsaved();
    void _testCancelledLinkApplyBlocksSave();
    void _testUnconfirmedLinkApplyBlocksSave();
    void _testSilentLinkApplyBlocksSave();
    void _testFailedHotspotApplyBlocksSave();
    void _testPendingStageOfAnotherLinkRidesAlong();
    void _testStreamPopupWithoutVehicle();
};

class CompanionVehicleLifecycleTest : public VehicleTestManualConnect
{
    Q_OBJECT

private slots:
    void _testDisconnectAndReconnectReset();
    void _testReadbackLinkHotspotAndOpOutlivesVehicle();
    void _testStreamOptionsFromCapabilityParams();
    void _testApplyProgressFromCompanionStages();
    void _testLinkApplyGapConfirmedByTelemetry();
    void _testHotspotApplyGapConfirmedByTelemetry();
    void _testSetWithoutAnyAckReportsNoResponse();
    void _testSecretNeverAppearsInOperationSteps();
    void _testStreamUrlRefusesInvalidAddress();
    void _testStreamUrlPathRule();
    void _testStreamFailureShowsOwnerReason();
    void _testStreamReasonTable();
    void _testLinkPortAndBaudApplyAndConflictGuard();
    void _testHotspotApplyAllowedWhenStageDiffers();
    void _testVisionConfidence();
    void _testStreamModeProgressAndVideoRestart();
    void _testStreamModeSilenceThreeSeconds();
    void _testStreamModeCancel();
    void _testCameraWriteAllowedWhileArmed();
    void _testInactiveGroupAckOnly();
    void _testStaleInProgressAfterFailedIgnored();
    void _testOffAsksWhenArmed();
    void _testModeChangeEndsStuckSettingsOp();
    void _testModeRefusedWhileVisionOpPending();
    void _testPlayerFollowsConfirmedMode();
    void _testReplaceIgnoresStaleKeepalive();
    void _testReplaceRefusedKeepsOldOp();
    void _testMessageProcessedOnce();
    void _testOtherSystemCompanionGetsNoConfig();
    void _testOtherVehicleResetsCompanionLock();
    void _testReplaceNotSentKeepsOldOp();
    void _testApplyWaitAcrossVehicleLoss();
    void _testUnsavedClearedWhenCompanionLockResets();
    void _testUnsavedKeptWhenSameCompanionReturns();
};
