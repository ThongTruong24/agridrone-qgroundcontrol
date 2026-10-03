#include "CompanionProtocolTest.h"
#include "CompanionProtocol.h"

#include <cstring>

UT_REGISTER_TEST(CompanionProtocolTest, TestLabel::Unit)

void CompanionProtocolTest::_testFromMavStringUnterminated()
{
    char full[4];
    std::memcpy(full, "ABCD", 4);  // no NUL
    QCOMPARE(Companion::fromMavString(full), QStringLiteral("ABCD"));

    char shortStr[8] = "ab";
    QCOMPARE(Companion::fromMavString(shortStr), QStringLiteral("ab"));
}

void CompanionProtocolTest::_testSendCommandWithoutLinkFails()
{
    QVERIFY(!Companion::sendCommand(nullptr, 1, MAV_CMD_THACO_APPLY_CONFIG, Companion::All));
}
