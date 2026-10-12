#pragma once

#include "UnitTest.h"

class CompanionMavlinkDispatcherTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testDispatchFansOutToAllHandlers();
    void _testNoDuplicateRegistration();
    void _testUnregisterStopsDelivery();
    void _testResetAllHandlers();
    void _testNullHandlerAndEmptyDispatch();
};
