#pragma once

#include "UnitTest.h"

class CompanionShellControllerTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testWireGoldenVectors();
    void _testInvalidEnvelopesRejected();
    void _testStateAndAckBodyVectors();
    void _testOuterFramingRoundTrip();
    void _testReplyAdmission();

    // controller (spec 2026-10-19-mav-shell-design.md)
    void _testSameSourceDifferentSessions();
    void _testStateIndependentOfOutput();
    void _testStopAndWaitRetries();
    void _testLineTransferChunks();
    void _testLostCommitAckDoesNotResubmit();
    void _testOldEpochIgnored();
    void _testOutputGapAndDecoder();
    void _testInterruptSuspendsOutputUntilAck();
    void _testQueryAfterPageReopen();
    void _testVehicleSwitchRetiresToken();
    void _testTwoLinksPinHeartbeatRoute();
    void _testFcLifecycleKeepsPinnedRoute();
    void _testDisconnectUsesOldLink();
    void _testRemovedLinkNeverFallsBack();
    void _testTextCap();
    void _testLoginRules();
    void _testSharedTransitionCases();
    void _testQueryReconcilesUncertain();
    void _testPluginOwnsOneController();

    // page (CompanionShellPage.qml)
    void _testPasswordFieldNeverUsesOutputPrompt();
    void _testReopenPageQueriesSameController();
    void _testClearPreservesAuth();
    void _testStateDisablesInput();
    void _testHistoryContainsCommandsOnly();
    void _testPageStateConstantsMatchEnum();
    void _testAnalyzePagesIncludeShell();
};
