#pragma once

#include "UnitTest.h"

class CompanionControllerTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _initialState_test();
    void _decodeLinksTelemetry_test();
    void _decodeAvailablePorts_test();
    void _decodeTransportType_test();
    void _ignoreOtherComponentId_test();
};
