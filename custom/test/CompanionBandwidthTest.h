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
};
