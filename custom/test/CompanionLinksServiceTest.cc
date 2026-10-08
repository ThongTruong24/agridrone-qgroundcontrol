#include "CompanionLinksServiceTest.h"

#include <QtTest/QSignalSpy>
#include <cstring>

#include "CompanionLinksService.h"
#include "QGCMAVLink.h"

UT_REGISTER_TEST(CompanionLinksServiceTest, TestLabel::Unit)

namespace {
template <std::size_t Size>
void copyField(char (&target)[Size], const char* source)
{
    std::strncpy(target, source, Size - 1);
}

mavlink_message_t packLink(uint8_t system, uint8_t component, const char* name, uint8_t index,
                           uint32_t baud, const char* port, uint8_t status, uint8_t count = 4)
{
    mavlink_cc_serial_link_t packet{};
    packet.link_index = index;
    packet.link_count = count;
    packet.baudrate = baud;
    packet.status = status;
    copyField(packet.name, name);
    copyField(packet.port, port);

    mavlink_message_t message{};
    mavlink_msg_cc_serial_link_encode(system, component, &message, &packet);
    return message;
}
}  // namespace

void CompanionLinksServiceTest::_testDecodeLinksMessage()
{
    CompanionLinksService service;
    QSignalSpy linksSpy(&service, &CompanionLinksService::linksChanged);

    const mavlink_message_t msgFc = packLink(42, 191, "FC", 0, 921600, "/dev/ttyFC", 2);
    const mavlink_message_t msgSiyi = packLink(42, 191, "SIYI", 1, 115200, "/dev/ttySIYI", 2);
    QVERIFY(service.handleMavlinkMessage(msgFc));
    QVERIFY(service.handleMavlinkMessage(msgSiyi));

    QVERIFY(service.hasLinksTelemetry());
    QCOMPARE(service.fcPort(), QStringLiteral("/dev/ttyFC"));
    QCOMPARE(service.siyiPort(), QStringLiteral("/dev/ttySIYI"));
    QCOMPARE(service.transportProtocol(), QStringLiteral("Serial / UART"));
    QVERIFY(linksSpy.count() >= 1);
}

void CompanionLinksServiceTest::_testTransportProtocolMapping()
{
    CompanionLinksService service;
    QCOMPARE(service.transportProtocol(), QStringLiteral("Serial / UART"));
}

void CompanionLinksServiceTest::_testRejectsWrongMessageId()
{
    CompanionLinksService service;

    mavlink_message_t heartbeat{};
    mavlink_msg_heartbeat_pack(1, 191, &heartbeat, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_PX4, 0, 0, MAV_STATE_ACTIVE);

    QVERIFY(!service.handleMavlinkMessage(heartbeat));
    QVERIFY(!service.hasLinksTelemetry());
}

void CompanionLinksServiceTest::_testRejectsWrongComponentId()
{
    CompanionLinksService service;

    // Component id 50 is neither the companion computer (191) nor broadcast (0).
    const mavlink_message_t message = packLink(42, 50, "FC", 0, 921600, "/dev/ttyFC", 2);

    QVERIFY(!service.handleMavlinkMessage(message));
    QVERIFY(!service.hasLinksTelemetry());
}

void CompanionLinksServiceTest::_testResetStateClears()
{
    CompanionLinksService service;
    QVERIFY(service.handleMavlinkMessage(packLink(42, 191, "FC", 0, 921600, "/dev/ttyFC", 2)));
    QVERIFY(service.hasLinksTelemetry());

    QSignalSpy linksSpy(&service, &CompanionLinksService::linksChanged);
    service.resetState();

    QVERIFY(!service.hasLinksTelemetry());
    QVERIFY(service.fcPort().isEmpty());
    QVERIFY(service.siyiPort().isEmpty());
    QCOMPARE(service.transportProtocol(), QStringLiteral("Serial / UART"));
    QVERIFY(linksSpy.count() >= 1);
}

void CompanionLinksServiceTest::_testIndexedNames()
{
    CompanionLinksService service;
    QVERIFY(service.handleMavlinkMessage(packLink(42, 191, "FC", 1, 115200, "/dev/portB", 2)));
    QVERIFY(service.handleMavlinkMessage(packLink(42, 191, "FC", 0, 921600, "/dev/portA", 2)));
    QVERIFY(service.handleMavlinkMessage(packLink(42, 191, "Extra", 3, 57600, "/dev/extra", 2)));
    QCOMPARE(service.fcPort(), QString("/dev/portA"));
    QCOMPARE(service.siyiPort(), QString("/dev/portB"));
    auto links = service.serialLinks();
    QCOMPARE(links.size(), 3);
    QCOMPARE(links[1].toMap()["name"].toString(), QString("FC"));
    QCOMPARE(links[2].toMap()["index"].toInt(), 3);
}

void CompanionLinksServiceTest::_testDropsLinksBeyondReportedCount()
{
    CompanionLinksService service;
    QVERIFY(service.handleMavlinkMessage(packLink(42, 191, "FC", 0, 921600, "/dev/ttyFC", 2, 2)));
    QVERIFY(service.handleMavlinkMessage(packLink(42, 191, "SIYI", 1, 115200, "/dev/ttySIYI", 2, 2)));
    QCOMPARE(service.serialLinks().size(), 2);

    // Companion now reports a single link: the stale second entry must disappear.
    QVERIFY(service.handleMavlinkMessage(packLink(42, 191, "FC", 0, 921600, "/dev/ttyFC", 2, 1)));
    QCOMPARE(service.serialLinks().size(), 1);
    QCOMPARE(service.serialLinks().first().toMap()["index"].toInt(), 0);
}
