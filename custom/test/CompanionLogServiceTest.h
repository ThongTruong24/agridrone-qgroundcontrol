#pragma once

#include "UnitTest.h"

class CompanionLogServiceTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testLogAddAndSignals();
    void _testCategoryFilterIsCaseInsensitive();
    void _testClearSingleCategory();
    void _testClearAll();
    void _testRingBufferCap();
};
