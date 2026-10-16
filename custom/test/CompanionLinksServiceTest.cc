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
                           uint32_t baud, const char* port, uint8_t status, uint8_t count = 4, uint32_t rxErrors = 0)
{
    mavlink_cc_serial_link_t packet{};
    packet.rx_errors = rxErrors;
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

    const mavlink_message_t msgFc = packLink(42, 191, "FC", 0, 921600, "/dev/ttyFC", 2, 4, 609);
    const mavlink_message_t msgSiyi = packLink(42, 191, "SIYI", 1, 115200, "/dev/ttySIYI", 2);
    QVERIFY(service.handleMavlinkMessage(msgFc));
    QVERIFY(service.handleMavlinkMessage(msgSiyi));

    QVERIFY(service.hasLinksTelemetry());
    // 2026-10-14: read per link from serialLinks(); the fcPort()/siyiPort() accessors are gone.
    QCOMPARE(service.serialLinks().at(0).toMap()["port"].toString(), QStringLiteral("/dev/ttyFC"));
    QCOMPARE(service.serialLinks().at(1).toMap()["port"].toString(), QStringLiteral("/dev/ttySIYI"));
    QCOMPARE(service.serialLinks().at(0).toMap()["baudrate"].toUInt(), 921600u);
    // 2026-10-15: a baud mismatch showed only B/s, never the UART errors the CC reports.
    QCOMPARE(service.serialLinks().at(0).toMap()["rxErrors"].toUInt(), 609u);
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
    QVERIFY(service.serialLinks().isEmpty()); // 2026-10-14: was fcPort()/siyiPort() empty
    QCOMPARE(service.transportProtocol(), QStringLiteral("Serial / UART"));
    QVERIFY(linksSpy.count() >= 1);
}

void CompanionLinksServiceTest::_testIndexedNames()
{
    CompanionLinksService service;
    QVERIFY(service.handleMavlinkMessage(packLink(42, 191, "FC", 1, 115200, "/dev/portB", 2)));
    QVERIFY(service.handleMavlinkMessage(packLink(42, 191, "FC", 0, 921600, "/dev/portA", 2)));
    QVERIFY(service.handleMavlinkMessage(packLink(42, 191, "Extra", 3, 57600, "/dev/extra", 2)));
    auto links = service.serialLinks(); // 2026-10-14: the fcPort()/siyiPort() checks are gone with the accessors
    QCOMPARE(links[0].toMap()["port"].toString(), QString("/dev/portA"));
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

// 2026-10-14: QGC assumed link 0 = FC and link 1 = SIYI; any link the CC reports is shown and logged by its own name.
void CompanionLinksServiceTest::_testAnyLinkByName()
{
    CompanionLinksService service;
    QSignalSpy logSpy(&service, &CompanionLinksService::logMessage);
    QVERIFY(service.handleMavlinkMessage(packLink(42, 191, "GPS", 2, 38400, "/dev/ttyUSB3", 2, 3)));
    const auto links = service.serialLinks();
    QCOMPARE(links.size(), 1);
    QCOMPARE(links.first().toMap()["index"].toInt(), 2);
    QCOMPARE(links.first().toMap()["name"].toString(), QString("GPS"));
    QCOMPARE(logSpy.count(), 1);
    QCOMPARE(logSpy.first().at(2).toString(), QString("[EVENT] GPS link -> ONLINE (/dev/ttyUSB3@38400 bps)"));
    QVERIFY(service.handleMavlinkMessage(packLink(42, 191, "GPS", 2, 38400, "/dev/ttyUSB3", 2, 3)));
    QCOMPARE(logSpy.count(), 1); // only a status change is logged
}
