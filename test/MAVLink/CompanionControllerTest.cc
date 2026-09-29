#include "CompanionControllerTest.h"
#include <QtTest/QSignalSpy>
#include <cstring>

#include "CompanionController.h"
#include "QGCMAVLink.h"

void CompanionControllerTest::_initialState_test()
{
    CompanionController controller;
    QVERIFY(!controller.hasLinksTelemetry());
    QCOMPARE(controller.fcTxRate(), 0.0f);
    QCOMPARE(controller.fcRxRate(), 0.0f);
    QCOMPARE(controller.fcRxLoss(), 0.0f);
    QCOMPARE(controller.transportProtocol(), QStringLiteral("Serial / UART"));
}

void CompanionControllerTest::_decodeLinksTelemetry_test()
{
    CompanionController controller;
    QSignalSpy spy(&controller, &CompanionController::linksChanged);

    mavlink_message_t msg{};
    mavlink_cc_telemetry_links_t lnk{};
    lnk.fc_tx_rate = 725.0f;
    lnk.fc_rx_rate = 14841.0f;
    lnk.fc_tx_rate_max = 787.0f;
    lnk.fc_tx_rate_multi = 1.0f;
    lnk.fc_rx_loss = 0.0f;
    lnk.fc_tx_err = 0;
    lnk.fc_bytes_rx = 7937536; // ~7.57 MB
    lnk.fc_bytes_tx = 350000;
    lnk.fc_baudrate = 921600;
    lnk.fc_status = 2; // CONNECTED / ONLINE
    strncpy(lnk.fc_port, "/dev/ttyAMA4", sizeof(lnk.fc_port) - 1);

    lnk.siyi_tx_rate = 250.0f;
    lnk.siyi_rx_rate = 3200.0f;
    lnk.siyi_tx_rate_max = 400.0f;
    lnk.siyi_tx_rate_multi = 1.0f;
    lnk.siyi_rx_loss = 1.5f;
    lnk.siyi_tx_err = 0;
    lnk.siyi_bytes_rx = 1200000;
    lnk.siyi_bytes_tx = 150000;
    lnk.siyi_baudrate = 115200;
    lnk.siyi_status = 2;
    strncpy(lnk.siyi_port, "/dev/ttyAMA0", sizeof(lnk.siyi_port) - 1);

    lnk.transport_type = 1; // Serial / UART
    strncpy(lnk.available_ports, "ttyAMA0,ttyAMA4", sizeof(lnk.available_ports) - 1);

    mavlink_msg_cc_telemetry_links_encode(1, 191, &msg, &lnk); // compid 191 = kCompanionCompId

    controller._onMavlinkMessageReceived(msg);

    QVERIFY(controller.hasLinksTelemetry());
    QCOMPARE(spy.count(), 1);

    // Verify FC link fields
    QCOMPARE(controller.fcTxRate(), 725.0f);
    QCOMPARE(controller.fcRxRate(), 14841.0f);
    QCOMPARE(controller.fcTxRateMax(), 787.0f);
    QCOMPARE(controller.fcTxRateMulti(), 1.0f);
    QCOMPARE(controller.fcRxLoss(), 0.0f);
    QCOMPARE(controller.fcTxErr(), 0u);
    QCOMPARE(controller.fcBytesRx(), 7937536u);
    QCOMPARE(controller.fcBytesTx(), 350000u);
    QCOMPARE(controller.fcBaud(), 921600);
    QCOMPARE(controller.fcPort(), QStringLiteral("/dev/ttyAMA4"));
    QCOMPARE(controller.fcStatus(), 2);

    // Verify SIYI link fields
    QCOMPARE(controller.siyiTxRate(), 250.0f);
    QCOMPARE(controller.siyiRxRate(), 3200.0f);
    QCOMPARE(controller.siyiRxLoss(), 1.5f);
    QCOMPARE(controller.siyiBaud(), 115200);
    QCOMPARE(controller.siyiPort(), QStringLiteral("/dev/ttyAMA0"));
    QCOMPARE(controller.siyiStatus(), 2);

    // Verify Transport Protocol
    QCOMPARE(controller.transportProtocol(), QStringLiteral("Serial / UART"));
}

void CompanionControllerTest::_decodeAvailablePorts_test()
{
    CompanionController controller;
    mavlink_message_t msg{};
    mavlink_cc_telemetry_links_t lnk{};
    strncpy(lnk.available_ports, "ttyAMA0,ttyAMA4,ttyUSB0", sizeof(lnk.available_ports) - 1);
    mavlink_msg_cc_telemetry_links_encode(1, 191, &msg, &lnk);

    controller._onMavlinkMessageReceived(msg);

    QStringList expected{"/dev/ttyAMA0", "/dev/ttyAMA4", "/dev/ttyUSB0"};
    QCOMPARE(controller.availablePorts(), expected);
}

void CompanionControllerTest::_decodeTransportType_test()
{
    CompanionController controller;

    mavlink_message_t msg{};
    mavlink_cc_telemetry_links_t lnk{};

    // UDP
    lnk.transport_type = 2;
    mavlink_msg_cc_telemetry_links_encode(1, 191, &msg, &lnk);
    controller._onMavlinkMessageReceived(msg);
    QCOMPARE(controller.transportProtocol(), QStringLiteral("UDP"));

    // TCP
    lnk.transport_type = 3;
    mavlink_msg_cc_telemetry_links_encode(1, 191, &msg, &lnk);
    controller._onMavlinkMessageReceived(msg);
    QCOMPARE(controller.transportProtocol(), QStringLiteral("TCP"));

    // Serial
    lnk.transport_type = 1;
    mavlink_msg_cc_telemetry_links_encode(1, 191, &msg, &lnk);
    controller._onMavlinkMessageReceived(msg);
    QCOMPARE(controller.transportProtocol(), QStringLiteral("Serial / UART"));
}

void CompanionControllerTest::_ignoreOtherComponentId_test()
{
    CompanionController controller;
    QSignalSpy spy(&controller, &CompanionController::linksChanged);

    mavlink_message_t msg{};
    mavlink_cc_telemetry_links_t lnk{};
    lnk.fc_rx_rate = 9999.0f;
    // compid 50 (not 191 and not 0)
    mavlink_msg_cc_telemetry_links_encode(1, 50, &msg, &lnk);

    controller._onMavlinkMessageReceived(msg);

    QVERIFY(!controller.hasLinksTelemetry());
    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.fcRxRate(), 0.0f);
}

UT_REGISTER_TEST(CompanionControllerTest, TestLabel::Unit)

