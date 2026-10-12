#pragma once

#include "UnitTest.h"

class CompanionLinksServiceTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testIndexedNames();
    void _testDecodeLinksMessage();
    void _testTransportProtocolMapping();
    void _testRejectsWrongMessageId();
    void _testRejectsWrongComponentId();
    void _testResetStateClears();
    void _testDropsLinksBeyondReportedCount();
};
