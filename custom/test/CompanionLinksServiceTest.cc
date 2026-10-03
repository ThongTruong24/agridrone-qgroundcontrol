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

mavlink_message_t packLinks(uint8_t system, uint8_t component, uint32_t fcBaud, uint32_t siyiBaud,
                            const char* fcPort, const char* siyiPort, uint8_t status, uint8_t transportType)
{
    mavlink_cc_telemetry_links_t packet{};
    packet.fc_baudrate = fcBaud;
    packet.siyi_baudrate = siyiBaud;
    packet.fc_status = status;
    packet.siyi_status = status;
    packet.transport_type = transportType;
    copyField(packet.fc_port, fcPort);
    copyField(packet.siyi_port, siyiPort);

    mavlink_message_t message{};
    mavlink_msg_cc_telemetry_links_encode(system, component, &message, &packet);
    return message;
}
}  // namespace

void CompanionLinksServiceTest::_testDecodeLinksMessage()
{
    CompanionLinksService service;
    QSignalSpy linksSpy(&service, &CompanionLinksService::linksChanged);

    const mavlink_message_t message = packLinks(42, 191, 921600, 115200, "/dev/ttyFC", "/dev/ttySIYI", 2, 1);
    QVERIFY(service.handleMavlinkMessage(message));

    QVERIFY(service.hasLinksTelemetry());
    QCOMPARE(service.fcPort(), QStringLiteral("/dev/ttyFC"));
    QCOMPARE(service.siyiPort(), QStringLiteral("/dev/ttySIYI"));
    QCOMPARE(service.transportProtocol(), QStringLiteral("Serial / UART"));
    QVERIFY(linksSpy.count() >= 1);
}

void CompanionLinksServiceTest::_testTransportProtocolMapping()
{
    CompanionLinksService service;

    QVERIFY(service.handleMavlinkMessage(packLinks(42, 191, 0, 0, "fc", "siyi", 2, 2)));
    QCOMPARE(service.transportProtocol(), QStringLiteral("UDP"));

    QVERIFY(service.handleMavlinkMessage(packLinks(42, 191, 0, 0, "fc", "siyi", 2, 3)));
    QCOMPARE(service.transportProtocol(), QStringLiteral("TCP"));

    // Unknown transport type falls back to the serial default.
    QVERIFY(service.handleMavlinkMessage(packLinks(42, 191, 0, 0, "fc", "siyi", 2, 99)));
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
    const mavlink_message_t message = packLinks(42, 50, 921600, 115200, "/dev/ttyFC", "/dev/ttySIYI", 2, 1);

    QVERIFY(!service.handleMavlinkMessage(message));
    QVERIFY(!service.hasLinksTelemetry());
}

void CompanionLinksServiceTest::_testResetStateClears()
{
    CompanionLinksService service;
    QVERIFY(service.handleMavlinkMessage(packLinks(42, 191, 921600, 115200, "/dev/ttyFC", "/dev/ttySIYI", 2, 2)));
    QVERIFY(service.hasLinksTelemetry());

    QSignalSpy linksSpy(&service, &CompanionLinksService::linksChanged);
    service.resetState();

    QVERIFY(!service.hasLinksTelemetry());
    QVERIFY(service.fcPort().isEmpty());
    QVERIFY(service.siyiPort().isEmpty());
    QCOMPARE(service.transportProtocol(), QStringLiteral("Serial / UART"));
    QVERIFY(linksSpy.count() >= 1);
}
