#pragma once
#include "UnitTest.h"

class CompanionProtocolTest : public UnitTest
{
    Q_OBJECT
private slots:
    void _testFromMavStringUnterminated();
    void _testSendCommandWithoutLinkFails();
};
