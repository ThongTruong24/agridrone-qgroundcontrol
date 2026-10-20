#pragma once

#include "UnitTest.h"

class CompanionBandwidthTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testMeanOfThreeCcSamples();
    void _testClearedBitIsNoData();
    void _testStates();
    void _testLocalRatesAndPairing();
    void _testEndpointForUdpConfig();
    void _testReconnectedLinkHasNoStaleRate();
    void _testLinkWithoutRateOmitsQgcRx();
    void _testValidFlagsNeedCcSession();
    void _testGapClearsWindow();
    void _testDuplicateDeliveryIgnored();
    void _testCounterDecreaseIsZeroAndRebaselines();
    void _testMeanSkipsClearedOwnerBit();
    void _testQgcWindowSlides();
};
