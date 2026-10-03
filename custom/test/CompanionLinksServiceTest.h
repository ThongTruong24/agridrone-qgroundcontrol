#pragma once

#include "UnitTest.h"

class CompanionLinksServiceTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testDecodeLinksMessage();
    void _testTransportProtocolMapping();
    void _testRejectsWrongMessageId();
    void _testRejectsWrongComponentId();
    void _testResetStateClears();
};
