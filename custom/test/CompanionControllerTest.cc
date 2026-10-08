#include "CompanionControllerTest.h"

#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtGui/QWindow>
#include <QtTest/QSignalSpy>
#include <cstring>
#include <memory>

#include "CompanionController.h"
#include "ColoredSvgImageProvider.h"
#include "MAVLinkProtocol.h"
#include "MockLink.h"
#include "QGCMAVLink.h"
#include "Vehicle.h"

UT_REGISTER_TEST(CompanionControllerTest, TestLabel::Unit, TestLabel::Vehicle)
UT_REGISTER_TEST(CompanionVehicleLifecycleTest, TestLabel::Integration, TestLabel::Vehicle)

namespace {
template <std::size_t Size>
void copyField(char (&target)[Size], const char* source)
{
    std::strncpy(target, source, Size - 1);
}

void packLinksForTest(uint8_t system, uint8_t component, mavlink_message_t* message, uint32_t fcBaud, uint32_t siyiBaud,
                      uint32_t fcBytesRx, uint32_t fcBytesTx, float fcRate, uint8_t status, const char* fcPort,
                      const char* siyiPort, mavlink_message_t* siyiMessage = nullptr)
{
    mavlink_cc_serial_link_t packet{};
    packet.link_index = 0;
    packet.link_count = 2;
    packet.baudrate = fcBaud;
    packet.rx_bytes = fcBytesRx;
    packet.tx_bytes = fcBytesTx;
    packet.tx_rate = fcRate;
    packet.status = status;
    copyField(packet.name, "FC");
    copyField(packet.port, fcPort);
    mavlink_msg_cc_serial_link_encode(system, component, message, &packet);

    if (siyiMessage) {
        mavlink_cc_serial_link_t siyiPacket{};
        siyiPacket.link_index = 1;
        siyiPacket.link_count = 2;
        siyiPacket.baudrate = siyiBaud;
        siyiPacket.rx_bytes = 0;
        siyiPacket.tx_bytes = 0;
        siyiPacket.tx_rate = 0.0f;
        siyiPacket.status = status;
        copyField(siyiPacket.name, "SIYI");
        copyField(siyiPacket.port, siyiPort);
        mavlink_msg_cc_serial_link_encode(system, component, siyiMessage, &siyiPacket);
    } else {
        (void)siyiBaud;
        (void)siyiPort;
    }
}

void packCameraForTest(mavlink_message_t* message)
{
    mavlink_cc_telemetry_camera_t packet{};
    packet.video_width = 1920;
    packet.video_height = 1080;
    packet.rotation = 90;
    packet.depth_width = 640;
    packet.depth_height = 480;
    packet.bitrate_kbps = 4000;
    packet.bitrate_max_kbps = 6000;
    packet.vbv_buffer_kb = 800;
    packet.video_fps = 30;
    packet.depth_fps = 15;
    packet.profile_mode = 2;
    packet.enable_emitter = 1;
    packet.usb_speed_mode = 3;
    copyField(packet.camera_type, "D455");
    copyField(packet.serial_number, "CAM-123");
    copyField(packet.codec, "h265");
    copyField(packet.encoder_mode, "cbr");
    copyField(packet.rtsp_url_qgc, "rtsp://10.0.0.2/live");
    mavlink_msg_cc_telemetry_camera_encode(42, 191, message, &packet);
}

void packNetworkForTest(mavlink_message_t* message)
{
    mavlink_cc_telemetry_network_t packet{};
    packet.ap_channel = 6;
    packet.ap_ieee80211n = 1;
    packet.ap_wmm_enabled = 1;
    packet.ap_wpa = 2;
    packet.ap_client_count = 4;
    packet.wlan0_dhcp = 1;
    packet.dnsmasq_status = 1;
    packet.ap_status = 1;
    copyField(packet.eth0_ip, "10.0.0.10");
    copyField(packet.eth0_netmask, "255.255.255.0");
    copyField(packet.wlan0_ip, "192.168.1.20");
    copyField(packet.ap_ip, "192.168.4.1");
    copyField(packet.ap_ssid, "AgriDrone");
    copyField(packet.ap_wpa_passphrase, "secret123");
    copyField(packet.ap_key_mgmt, "WPA-PSK");
    copyField(packet.ap_hw_mode, "g");
    mavlink_msg_cc_telemetry_network_encode(42, 191, message, &packet);
}
}  // namespace

void CompanionControllerTest::_testGeneratedMessageDecoding()
{
    CompanionController controller;
    controller.resetForTest();
    QSignalSpy linksChangedSpy(&controller, &CompanionController::linksChanged);
    QSignalSpy cameraChangedSpy(&controller, &CompanionController::cameraChanged);
    QSignalSpy networkChangedSpy(&controller, &CompanionController::networkChanged);
    QSignalSpy visionChangedSpy(&controller, &CompanionController::visionChanged);
    QSignalSpy systemChangedSpy(&controller, &CompanionController::systemChanged);

    mavlink_message_t message{};
    mavlink_message_t siyiMsg{};
    packLinksForTest(42, 191, &message, 921600, 115200, 123456, 654321, 88.5F, 3, "ttyUSB0", "ttyUSB1", &siyiMsg);
    controller.processMessageForTest(message);
    controller.processMessageForTest(siyiMsg);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_baudrate")).toULongLong(), 921600ULL);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("siyi_baudrate")).toULongLong(), 115200ULL);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_bytes_rx")).toULongLong(), 123456ULL);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_bytes_tx")).toULongLong(), 654321ULL);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_tx_rate")).toFloat(), 88.5F);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_status")).toInt(), 3);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_port")).toString(), QStringLiteral("ttyUSB0"));
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("siyi_port")).toString(), QStringLiteral("ttyUSB1"));
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("transport_type")).toInt(), 1);
    QVERIFY(controller.linksReceived());
    QVERIFY(linksChangedSpy.count() >= 1);

    packCameraForTest(&message);
    controller.processMessageForTest(message);
    QCOMPARE(controller.ccTelemetryCamera().value(QStringLiteral("video_width")).toInt(), 1920);
    QCOMPARE(controller.ccTelemetryCamera().value(QStringLiteral("video_height")).toInt(), 1080);
    QCOMPARE(controller.ccTelemetryCamera().value(QStringLiteral("rotation")).toInt(), 90);
    QCOMPARE(controller.ccTelemetryCamera().value(QStringLiteral("depth_width")).toInt(), 640);
    QCOMPARE(controller.ccTelemetryCamera().value(QStringLiteral("depth_height")).toInt(), 480);
    QCOMPARE(controller.ccTelemetryCamera().value(QStringLiteral("bitrate_kbps")).toInt(), 4000);
    QCOMPARE(controller.ccTelemetryCamera().value(QStringLiteral("bitrate_max_kbps")).toInt(), 6000);
    QCOMPARE(controller.ccTelemetryCamera().value(QStringLiteral("vbv_buffer_kb")).toInt(), 800);
    QCOMPARE(controller.ccTelemetryCamera().value(QStringLiteral("video_fps")).toInt(), 30);
    QCOMPARE(controller.ccTelemetryCamera().value(QStringLiteral("depth_fps")).toInt(), 15);
    QCOMPARE(controller.ccTelemetryCamera().value(QStringLiteral("profile_mode")).toInt(), 2);
    QCOMPARE(controller.ccTelemetryCamera().value(QStringLiteral("enable_emitter")).toInt(), 1);
    QCOMPARE(controller.ccTelemetryCamera().value(QStringLiteral("camera_type")).toString(), QStringLiteral("D455"));
    QCOMPARE(controller.ccTelemetryCamera().value(QStringLiteral("serial_number")).toString(),
             QStringLiteral("CAM-123"));
    QCOMPARE(controller.ccTelemetryCamera().value(QStringLiteral("codec")).toString(), QStringLiteral("h265"));
    QCOMPARE(controller.ccTelemetryCamera().value(QStringLiteral("encoder_mode")).toString(), QStringLiteral("cbr"));
    QCOMPARE(controller.ccTelemetryCamera().value(QStringLiteral("rtsp_url_qgc")).toString(),
             QStringLiteral("rtsp://10.0.0.2/live"));
    QCOMPARE(controller.ccTelemetryCamera().value(QStringLiteral("usb_speed_mode")).toInt(), 3);
    QVERIFY(controller.cameraReceived());
    QCOMPARE(cameraChangedSpy.count(), 1);

    packNetworkForTest(&message);
    controller.processMessageForTest(message);
    QCOMPARE(controller.ccTelemetryNetwork().value(QStringLiteral("ap_channel")).toInt(), 6);
    QCOMPARE(controller.ccTelemetryNetwork().value(QStringLiteral("ap_ieee80211n")).toInt(), 1);
    QCOMPARE(controller.ccTelemetryNetwork().value(QStringLiteral("ap_wmm_enabled")).toInt(), 1);
    QCOMPARE(controller.ccTelemetryNetwork().value(QStringLiteral("ap_wpa")).toInt(), 2);
    QCOMPARE(controller.ccTelemetryNetwork().value(QStringLiteral("ap_client_count")).toInt(), 4);
    QCOMPARE(controller.ccTelemetryNetwork().value(QStringLiteral("wlan0_dhcp")).toInt(), 1);
    QCOMPARE(controller.ccTelemetryNetwork().value(QStringLiteral("dnsmasq_status")).toInt(), 1);
    QCOMPARE(controller.ccTelemetryNetwork().value(QStringLiteral("eth0_ip")).toString(), QStringLiteral("10.0.0.10"));
    QCOMPARE(controller.ccTelemetryNetwork().value(QStringLiteral("wlan0_ip")).toString(),
             QStringLiteral("192.168.1.20"));
    QCOMPARE(controller.ccTelemetryNetwork().value(QStringLiteral("ap_ip")).toString(), QStringLiteral("192.168.4.1"));
    QCOMPARE(controller.ccTelemetryNetwork().value(QStringLiteral("ap_ssid")).toString(), QStringLiteral("AgriDrone"));
    QCOMPARE(controller.ccTelemetryNetwork().value(QStringLiteral("ap_wpa_passphrase")).toString(),
             QStringLiteral("secret123"));
    QCOMPARE(controller.ccTelemetryNetwork().value(QStringLiteral("ap_key_mgmt")).toString(),
             QStringLiteral("WPA-PSK"));
    QCOMPARE(controller.ccTelemetryNetwork().value(QStringLiteral("ap_hw_mode")).toString(), QStringLiteral("g"));
    QCOMPARE(controller.ccTelemetryNetwork().value(QStringLiteral("ap_status")).toInt(), 1);
    QCOMPARE(controller.ccTelemetryNetwork().value(QStringLiteral("eth0_netmask")).toString(),
             QStringLiteral("255.255.255.0"));
    QVERIFY(controller.networkReceived());
    QCOMPARE(networkChangedSpy.count(), 1);

    mavlink_msg_cc_telemetry_vision_pack(42, 191, &message, 0.65F, 24.5F, 640, 640, 30, 7, 3, "weed-v2", "rgb", 0, 0, 0, "", 0, 0);
    controller.processMessageForTest(message);
    QCOMPARE(controller.ccTelemetryVision().value(QStringLiteral("confidence_thresh")).toFloat(), 0.65F);
    QCOMPARE(controller.ccTelemetryVision().value(QStringLiteral("inference_fps")).toFloat(), 24.5F);
    QCOMPARE(controller.ccTelemetryVision().value(QStringLiteral("input_width")).toInt(), 640);
    QCOMPARE(controller.ccTelemetryVision().value(QStringLiteral("input_height")).toInt(), 640);
    QCOMPARE(controller.ccTelemetryVision().value(QStringLiteral("video_fps")).toInt(), 30);
    QCOMPARE(controller.ccTelemetryVision().value(QStringLiteral("detections_count")).toInt(), 7);
    QCOMPARE(controller.ccTelemetryVision().value(QStringLiteral("status_flags")).toInt(), 3);
    QCOMPARE(controller.ccTelemetryVision().value(QStringLiteral("model_name")).toString(), QStringLiteral("weed-v2"));
    QCOMPARE(controller.ccTelemetryVision().value(QStringLiteral("input_source")).toString(), QStringLiteral("rgb"));
    QVERIFY(controller.visionReceived());
    QCOMPARE(visionChangedSpy.count(), 1);
    QCOMPARE(controller.sourceSystemId(), 42);
    QCOMPARE(controller.sourceComponentId(), 191);
    mavlink_cc_telemetry_system_t system{};
    system.system_uptime_s = 3600;
    system.cpu_usage = 37;
    system.ram_usage = 48;
    system.disk_usage = 59;
    system.cpu_temp = 64;
    system.system_status = 1;
    mavlink_msg_cc_telemetry_system_encode(42, 191, &message, &system);
    controller.processMessageForTest(message);
    QCOMPARE(controller.ccTelemetrySystem().value(QStringLiteral("cpu_temp")).toInt(), 64);
    QCOMPARE(controller.ccTelemetrySystem().value(QStringLiteral("system_uptime_s")).toULongLong(), 3600ULL);
    QVERIFY(controller.systemReceived());
    QCOMPARE(systemChangedSpy.count(), 1);
}

void CompanionControllerTest::_testSourceFiltering()
{
    CompanionController controller;
    controller.resetForTest();

    mavlink_message_t message{};
    mavlink_msg_heartbeat_pack(42, 1, &message, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_GENERIC, 0, 0, MAV_STATE_ACTIVE);
    controller.processMessageForTest(message);
    QCOMPARE(controller.sourceComponentId(), -1);

    packLinksForTest(42, 191, &message, 57600, 115200, 10, 20, 2.5F, 1, "primary", "camera");
    controller.processMessageForTest(message);
    QCOMPARE(controller.sourceComponentId(), 191);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_port")).toString(), QStringLiteral("primary"));

    packLinksForTest(42, 192, &message, 9600, 9600, 99, 99, 1.0F, 0, "wrong", "wrong");
    controller.processMessageForTest(message);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_port")).toString(), QStringLiteral("primary"));

    controller.resetForTest();
    controller.processMessageForTest(message);
    QVERIFY(!controller.linksReceived());
}

void CompanionControllerTest::_testStaleAndReset()
{
    CompanionController controller;
    controller.resetForTest();
    QVERIFY(!controller.linksReceived());
    QVERIFY(!controller.linksStale());

    mavlink_message_t message{};
    packLinksForTest(1, 191, &message, 57600, 115200, 10, 20, 2.5F, 1, "primary", "camera");
    controller.processMessageForTest(message);
    QVERIFY(controller.linksReceived());
    QVERIFY(!controller.linksStale());

    controller.forceStaleForTest();
    QVERIFY(controller.linksStale());

    controller.resetForTest();
    QVERIFY(!controller.linksReceived());
    QVERIFY(!controller.linksStale());
    QCOMPARE(controller.sourceSystemId(), -1);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_port")).toString(), QString());
}

void CompanionControllerTest::_testCompanionPageLoads()
{
    ignoreLogMessage("default", QtWarningMsg,
                     QRegularExpression(QStringLiteral("QFontDatabase: Cannot find font directory.*")));
    // The headless test engine has no real window geometry, so the equal-width tab
    // buttons (width: tabBar.width / 5) emit benign TabBar/TabButton binding-loop
    // warnings that Qt resolves on its own. They do not affect the page instantiating.
    ignoreLogMessage("default", QtWarningMsg,
                     QRegularExpression(QStringLiteral(".*Binding loop detected.*")));
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qml"));
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/Custom/AgriDrone/CompanionSettings.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> page(component.create());
    QVERIFY2(page, qPrintable(component.errorString()));
}

void CompanionControllerTest::_testMissionAndLogFiltering()
{
    CompanionController controller;
    controller.resetForTest();
    mavlink_message_t message{};
    mavlink_msg_thaco_external_xyz_trigger_pack(42, 1, &message, 123, 4567);
    controller.processMessageForTest(message);
    QVERIFY(controller.missionReceived());
    QCOMPARE(controller.lastTriggerId(), 123);
    QCOMPARE(controller.lastTriggerTimeBootMs(), 4567);
    QVERIFY(controller.logMatchesFilter(QStringLiteral("MISSION:"), QStringLiteral("ALL"),
                                        QStringLiteral("MISSION: trigger 123")));
    QVERIFY(!controller.logMatchesFilter(QStringLiteral("MISSION:"), QStringLiteral("ALL"),
                                         QStringLiteral("FC: connected")));
}

void CompanionVehicleLifecycleTest::_testDisconnectAndReconnectReset()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    QVERIFY(vehicle());
    QVERIFY(controller.vehicleAvailable());
    const int firstEpoch = controller.vehicleEpoch();

    mavlink_message_t message{};
    packLinksForTest(vehicle()->id(), 191, &message, 921600, 115200, 10, 20, 4.5F, 3, "first", "siyi");
    emit vehicle() -> mavlinkMessageReceived(message);
    QVERIFY(controller.linksReceived());
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_port")).toString(), QStringLiteral("first"));

    packLinksForTest(vehicle()->id() + 1, 191, &message, 9600, 9600, 1, 1, 0.0F, 0, "wrong", "wrong");
    emit vehicle() -> mavlinkMessageReceived(message);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_port")).toString(), QStringLiteral("first"));

    _disconnectMockLink();
    QVERIFY(controller.vehicleEpoch() > firstEpoch);
    QVERIFY(!controller.vehicleAvailable());
    QVERIFY(!controller.linksReceived());
    QCOMPARE(controller.sourceComponentId(), -1);

    _connectMockLinkNoInitialConnectSequence();
    QVERIFY(vehicle());
    QVERIFY(controller.vehicleAvailable());
    QVERIFY(!controller.linksReceived());

    packLinksForTest(vehicle()->id(), 191, &message, 57600, 57600, 30, 40, 1.5F, 1, "second", "siyi2");
    emit vehicle() -> mavlinkMessageReceived(message);
    QCOMPARE(controller.sourceComponentId(), 191);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_port")).toString(), QStringLiteral("second"));
}

void CompanionVehicleLifecycleTest::_testUartAckFilteringAndRetry()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    QVERIFY(vehicle());

    mavlink_message_t message{};
    mavlink_message_t siyiMsg{};
    packLinksForTest(vehicle()->id(), 191, &message, 921600, 115200, 10, 20, 4.5F, 3, "fc-current", "siyi-current", &siyiMsg);
    emit vehicle() -> mavlinkMessageReceived(message);
    emit vehicle() -> mavlinkMessageReceived(siyiMsg);
    QCOMPARE(controller.availablePorts(), (QStringList{QStringLiteral("fc-current"), QStringLiteral("siyi-current")}));

    controller.applyLinksConfig(QStringLiteral("fc-new"), 460800, QStringLiteral("siyi-new"), 57600);
    QCOMPARE(controller.configStatus(), QStringLiteral("Applying"));
    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_CC_SERIAL_LINK) >= 1, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavCommandCount(static_cast<MAV_CMD>(44011), 191) >= 1, 3000);

    mavlink_message_t sent{};
    QVERIFY(mockLink()->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_CC_SERIAL_LINK, sent));
    mavlink_cc_serial_link_t sentLinks{};
    mavlink_msg_cc_serial_link_decode(&sent, &sentLinks);
    QCOMPARE(sentLinks.baudrate, 57600U);
    QCOMPARE(sentLinks.status, 3);
    QCOMPARE(QString::fromLatin1(sentLinks.port), QStringLiteral("siyi-new"));

    QVERIFY(mockLink()->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_COMMAND_LONG, sent));
    mavlink_command_long_t sentCommand{};
    mavlink_msg_command_long_decode(&sent, &sentCommand);
    QCOMPARE(sentCommand.command, 44011);
    QCOMPARE(sentCommand.target_system, vehicle()->id());
    QCOMPARE(sentCommand.target_component, 191);
    QCOMPARE(sentCommand.param1, 1.0F);
    QCOMPARE(sentCommand.param2, 1.0F);

    const auto sendAck = [&](quint8 systemId, quint8 componentId, quint16 command, MAV_RESULT result) {
        mavlink_message_t ack{};
        mavlink_msg_command_ack_pack(systemId, componentId, &ack, command, result, 0, 0,
                                     MAVLinkProtocol::instance()->getSystemId(), MAVLinkProtocol::getComponentId());
        emit vehicle() -> mavlinkMessageReceived(ack);
    };

    sendAck(vehicle()->id(), 191, 44010, MAV_RESULT_ACCEPTED);
    sendAck(vehicle()->id() + 1, 191, 44011, MAV_RESULT_ACCEPTED);
    sendAck(vehicle()->id(), 192, 44011, MAV_RESULT_ACCEPTED);
    mavlink_message_t wrongTargetAck{};
    mavlink_msg_command_ack_pack(vehicle()->id(), 191, &wrongTargetAck, 44011, MAV_RESULT_ACCEPTED, 0, 0,
                                 MAVLinkProtocol::instance()->getSystemId() == 1 ? 2 : 1,
                                 MAVLinkProtocol::getComponentId());
    emit vehicle() -> mavlinkMessageReceived(wrongTargetAck);
    QCOMPARE(controller.configStatus(), QStringLiteral("Applying"));

    sendAck(vehicle()->id(), 191, 44011, MAV_RESULT_DENIED);
    QCOMPARE(controller.configStatus(), QStringLiteral("Failed"));
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_port")).toString(), QStringLiteral("fc-current"));

    controller.applyLinksConfig(QStringLiteral("fc-new"), 460800, QStringLiteral("siyi-new"), 57600);
    QCOMPARE(controller.configStatus(), QStringLiteral("Applying"));
    controller._configTimedOut();
    QCOMPARE(controller.configStatus(), QStringLiteral("Timeout"));

    controller.applyLinksConfig(QStringLiteral("fc-new"), 460800, QStringLiteral("siyi-new"), 57600);
    QCOMPARE(controller.configStatus(), QStringLiteral("Applying"));
    sendAck(vehicle()->id(), 191, 44011, MAV_RESULT_IN_PROGRESS);
    QCOMPARE(controller.configStatus(), QStringLiteral("Applying"));
    sendAck(vehicle()->id(), 191, 44011, MAV_RESULT_ACCEPTED);
    QCOMPARE(controller.configStatus(), QStringLiteral("WaitingTelemetry"));
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_port")).toString(), QStringLiteral("fc-current"));

    controller.saveLinksConfig();
    QCOMPARE(controller.configStatus(), QStringLiteral("WaitingTelemetry"));
    controller._confirmationTimedOut();
    QCOMPARE(controller.configStatus(), QStringLiteral("Timeout"));

    controller.applyLinksConfig(QStringLiteral("port-name-longer-than-fifteen"), 460800, QStringLiteral("siyi-new"),
                                57600);
    QCOMPARE(controller.configStatus(), QStringLiteral("Failed"));
    QCOMPARE(controller.configMessage(), QStringLiteral("UART port must fit within 15 UTF-8 bytes"));

    controller.applyLinksConfig(QStringLiteral("fc-new"), 460800, QStringLiteral("siyi-new"), 57600);
    sendAck(vehicle()->id(), 191, 44011, MAV_RESULT_ACCEPTED);
    QCOMPARE(controller.configStatus(), QStringLiteral("WaitingTelemetry"));
    mavlink_message_t siyiTelemetryMsg{};
    packLinksForTest(vehicle()->id(), 191, &message, 460800, 57600, 10, 20, 4.5F, 3, "fc-new", "siyi-new", &siyiTelemetryMsg);
    emit vehicle() -> mavlinkMessageReceived(message);
    emit vehicle() -> mavlinkMessageReceived(siyiTelemetryMsg);
    QCOMPARE(controller.configStatus(), QStringLiteral("Success"));

    controller.saveLinksConfig();
    QCOMPARE(controller.configStatus(), QStringLiteral("Applying"));
    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavCommandCount(static_cast<MAV_CMD>(44010), 191) >= 1, 3000);
    QVERIFY(mockLink()->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_COMMAND_LONG, sent));
    mavlink_msg_command_long_decode(&sent, &sentCommand);
    QCOMPARE(sentCommand.command, 44010);
    QCOMPARE(sentCommand.target_system, vehicle()->id());
    QCOMPARE(sentCommand.target_component, 191);
    QCOMPARE(sentCommand.param1, 1.0F);
    QCOMPARE(sentCommand.param2, 0.0F);
    sendAck(vehicle()->id(), 191, 44010, MAV_RESULT_ACCEPTED);
    QCOMPARE(controller.configStatus(), QStringLiteral("Success"));
}

void CompanionVehicleLifecycleTest::_testQmlDraftAndSaveGuard()
{
    ignoreLogMessage("default", QtWarningMsg,
                     QRegularExpression(QStringLiteral("QFontDatabase: Cannot find font directory.*")));
    ignoreLogMessage("default", QtWarningMsg,
                     QRegularExpression(QStringLiteral(".*Invalid image provider: image://coloredsvg/.*")));
    _connectMockLinkNoInitialConnectSequence();
    QVERIFY(vehicle());

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qml"));
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/Custom/AgriDrone/CompanionSettings.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> page(component.create());
    QVERIFY2(page, qPrintable(component.errorString()));
    QObject* const tab = page->findChild<QObject*>(QStringLiteral("companionTelemetryTab"));
    QObject* const saveButton = page->findChild<QObject*>(QStringLiteral("saveUartButton"));
    QVERIFY(tab);
    QVERIFY(saveButton);

    mavlink_message_t telemetry{};
    mavlink_message_t siyiTelemetry{};
    packLinksForTest(vehicle()->id(), 191, &telemetry, 921600, 115200, 10, 20, 4.5F, 3, "fc-current", "siyi-current", &siyiTelemetry);
    emit vehicle() -> mavlinkMessageReceived(telemetry);
    emit vehicle() -> mavlinkMessageReceived(siyiTelemetry);
    QTRY_VERIFY_WITH_TIMEOUT(saveButton->property("enabled").toBool(), 1000);

    QVERIFY(tab->setProperty("_selectedFcPort", QStringLiteral("fc-new")));
    QVERIFY(tab->setProperty("_selectedFcBaud", 460800));
    QVERIFY(tab->setProperty("_userInteractedFc", true));
    QVERIFY(tab->setProperty("_selectedSiyiPort", QStringLiteral("siyi-new")));
    QVERIFY(tab->setProperty("_selectedSiyiBaud", 57600));
    QVERIFY(tab->setProperty("_userInteractedSiyi", true));
    QTRY_VERIFY_WITH_TIMEOUT(!saveButton->property("enabled").toBool(), 1000);

    QVERIFY(QMetaObject::invokeMethod(tab, "sendFcDraft", Q_ARG(QVariant, QVariant(QStringLiteral("fc-new"))),
                                      Q_ARG(QVariant, QVariant(460800))));
    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavCommandCount(static_cast<MAV_CMD>(44011), 191) >= 1, 3000);
    mavlink_message_t ack{};
    mavlink_msg_command_ack_pack(vehicle()->id(), 191, &ack, 44011, MAV_RESULT_ACCEPTED, 0, 0,
                                 MAVLinkProtocol::instance()->getSystemId(), MAVLinkProtocol::getComponentId());
    emit vehicle() -> mavlinkMessageReceived(ack);
    packLinksForTest(vehicle()->id(), 191, &telemetry, 460800, 115200, 10, 20, 4.5F, 3, "fc-new", "siyi-current", &siyiTelemetry);
    emit vehicle() -> mavlinkMessageReceived(telemetry);
    emit vehicle() -> mavlinkMessageReceived(siyiTelemetry);
    QTRY_VERIFY_WITH_TIMEOUT(!tab->property("_userInteractedFc").toBool(), 1000);
    QVERIFY(tab->property("_userInteractedSiyi").toBool());
    QCOMPARE(tab->property("_selectedSiyiPort").toString(), QStringLiteral("siyi-new"));
    QVERIFY(!saveButton->property("enabled").toBool());

    QVERIFY(QMetaObject::invokeMethod(tab, "sendSiyiDraft", Q_ARG(QVariant, QVariant(QStringLiteral("siyi-new"))),
                                      Q_ARG(QVariant, QVariant(57600))));
    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavCommandCount(static_cast<MAV_CMD>(44011), 191) >= 2, 3000);
    emit vehicle() -> mavlinkMessageReceived(ack);
    packLinksForTest(vehicle()->id(), 191, &telemetry, 460800, 57600, 10, 20, 4.5F, 3, "fc-new", "siyi-new", &siyiTelemetry);
    emit vehicle() -> mavlinkMessageReceived(telemetry);
    emit vehicle() -> mavlinkMessageReceived(siyiTelemetry);
    QTRY_VERIFY_WITH_TIMEOUT(saveButton->property("enabled").toBool(), 1000);

    QVERIFY(tab->setProperty("_selectedFcPort", QStringLiteral("unsaved")));
    QVERIFY(tab->setProperty("_userInteractedFc", true));
    QVERIFY(!saveButton->property("enabled").toBool());
    _disconnectMockLink();
    QTRY_VERIFY_WITH_TIMEOUT(!tab->property("_userInteractedFc").toBool(), 1000);
    QCOMPARE(tab->property("_selectedFcPort").toString(), QString());
}

void CompanionControllerTest::_testToolbarLegacyAndIndependentStreams()
{
    CompanionController controller;
    mavlink_cc_telemetry_camera_t camera{};
    camera.video_width = 1280; camera.video_height = 720; camera.video_fps = 30;
    mavlink_message_t message{};
    mavlink_msg_cc_telemetry_camera_encode(42, 191, &message, &camera);
    controller.processMessageForTest(message);
    QVERIFY(!controller.cameraStreamState()["available"].toBool());
    camera.config_version = 1; camera.fpv_enabled = 1; camera.publish_status = 0;
    mavlink_msg_cc_telemetry_camera_encode(42, 191, &message, &camera);
    controller.processMessageForTest(message);
    QVERIFY(controller.cameraStreamState()["available"].toBool());
    QVERIFY(controller.cameraStreamState()["enabled"].toBool());
    QVERIFY(!controller.visionStreamState()["available"].toBool());
    mavlink_cc_telemetry_vision_t vision{};
    vision.config_version = 1; vision.enabled = 1; vision.inference_fps = 4.8F;
    vision.video_fps = 15; vision.input_width = 640; vision.input_height = 480;
    mavlink_msg_cc_telemetry_vision_encode(42, 191, &message, &vision);
    controller.processMessageForTest(message);
    char fpsValue[128]{};
    const float configuredFps = 5.0F;
    std::memcpy(fpsValue, &configuredFps, sizeof(configuredFps));
    mavlink_msg_param_ext_value_pack(42, 191, &message, "CC_VIS_FPS", fpsValue, MAV_PARAM_EXT_TYPE_REAL32, 1, 0);
    controller.processMessageForTest(message);
    const auto state = controller.visionStreamState();
    QVERIFY(qAbs(state["current"].toMap()["fps"].toDouble() - 4.8) < 0.001);
    QCOMPARE(state["configured"].toMap()["fps"].toDouble(), 5.0);
    QVERIFY(!controller.connectStream("camera", "rtmp://invalid"));
    QVERIFY(!controller.setStreamParameter("vision", "CC_CAM_FPS", 15));
}

void CompanionControllerTest::_testToolbarPopupsLoad()
{
    QQmlEngine engine;
    engine.addImageProvider(QLatin1String(ColoredSvgImageProvider::ProviderId), new ColoredSvgImageProvider());
    engine.addImportPath("qrc:/qml");
    for (const auto &name : {"StreamStatusPage", "CompanionStatusPage"}) {
        QQmlComponent component(&engine, QUrl(QString("qrc:/qml/Custom/AgriDrone/%1.qml").arg(name)));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> page(component.createWithInitialProperties({{"expanded", true}}));
        QVERIFY2(page, qPrintable(component.errorString()));
        if (QString(name) == "StreamStatusPage") {
            auto url = page->findChild<QObject*>("rtspDestination");
            QVERIFY(url);
            url->setProperty("text", "rtsp://localhost:8554/draft");
            QVERIFY(QMetaObject::invokeMethod(url, "textEdited"));
            QCOMPARE(page->property("draftUrl").toString(), QString("rtsp://localhost:8554/draft"));
            QVERIFY(page->property("urlDirty").toBool());
        }
    }
    QQmlComponent windowComponent(&engine);
    windowComponent.setData(R"QML(
import QtQuick
import QtQuick.Window
import Custom.AgriDrone
Window {
    id: mainWindow
    width: 800; height: 600; visible: true
    property var popup: null
    property string opened: ""
    property bool urlFocused: false
    function showIndicatorDrawer(component, indicator) {
        if (popup) popup.destroy()
        popup = component.createObject(contentItem, {expanded: true})
        opened = popup.title || "Vehicle Companion Status"
        if (popup.draftUrl !== undefined) {
            var edit = findItem(popup, "rtspDestination")
            edit.forceActiveFocus()
            urlFocused = edit.activeFocus
        }
    }
    function findItem(parent, name) {
        if (parent.objectName === name) return parent
        for (var i = 0; i < parent.children.length; ++i) {
            var result = findItem(parent.children[i], name)
            if (result) return result
        }
        return null
    }
    function openCamera() { camera.children[1].clicked(null) }
    function openVision() { vision.children[1].clicked(null) }
    function openCompanion() { companion.children[1].clicked(null) }
    Row {
        width: parent.width; height: 50
        CameraStatusIndicator { id: camera }
        VisionStatusIndicator { id: vision }
        CompanionStatusIndicator { id: companion }
    }
}
)QML", QUrl("qrc:/qml/toolbar-window-test.qml"));
    QVERIFY2(windowComponent.isReady(), qPrintable(windowComponent.errorString()));
    std::unique_ptr<QObject> window(windowComponent.create());
    QVERIFY2(window, qPrintable(windowComponent.errorString()));
    QVERIFY(QTest::qWaitForWindowExposed(qobject_cast<QWindow *>(window.get())));
    QVERIFY(QMetaObject::invokeMethod(window.get(), "openCamera"));
    QCOMPARE(window->property("opened").toString(), QString("Vehicle Camera Status"));
    QVERIFY(window->property("urlFocused").toBool());
    QVERIFY(QMetaObject::invokeMethod(window.get(), "openVision"));
    QCOMPARE(window->property("opened").toString(), QString("Vehicle Vision Status"));
    QVERIFY(QMetaObject::invokeMethod(window.get(), "openCompanion"));
    QCOMPARE(window->property("opened").toString(), QString("Vehicle Companion Status"));
}

void CompanionVehicleLifecycleTest::_testConnectUrlAckBeforeEnableAndVehicleCancel()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    QVERIFY(vehicle());
    const auto system = vehicle()->id();
    mavlink_cc_telemetry_camera_t camera{};
    camera.config_version = 1; camera.fpv_enabled = 1; camera.supported_codecs = 1;
    mavlink_message_t message{};
    mavlink_msg_cc_telemetry_camera_encode(system, 191, &message, &camera);
    controller.processMessageForTest(message);
    {
        QQmlEngine readEngine;
        readEngine.addImageProvider(QLatin1String(ColoredSvgImageProvider::ProviderId), new ColoredSvgImageProvider());
        readEngine.addImportPath("qrc:/qml");
        QQmlComponent readComponent(&readEngine);
        readComponent.setData(R"QML(
import QtQuick
import QGroundControl
import Custom.AgriDrone
Item {
    property var controller: CompanionController
    StreamStatusPage { expanded: true }
}
)QML", QUrl("qrc:/qml/toolbar-readback-test.qml"));
        QVERIFY2(readComponent.isReady(), qPrintable(readComponent.errorString()));
        std::unique_ptr<QObject> readPage(readComponent.create());
        QVERIFY2(readPage, qPrintable(readComponent.errorString()));
        auto uiController = qobject_cast<CompanionController *>(readPage->property("controller").value<QObject *>());
        QVERIFY(uiController);
        const int readsBefore = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_REQUEST_LIST);
        const int writesBefore = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
        uiController->processMessageForTest(message);
        QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_REQUEST_LIST), readsBefore + 1, 1500);
        uiController->processMessageForTest(message);
        QCOMPARE(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_REQUEST_LIST), readsBefore + 1);
        QCOMPARE(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), writesBefore);
    }
    const auto value = [&](const char *id, const char *url, uint8_t publish, bool ack, uint8_t result, uint8_t component = 191) {
        char raw[128]{};
        const bool string = QString(id).endsWith("URL");
        if (string) std::strncpy(raw, url, sizeof(raw) - 1); else raw[0] = publish;
        mavlink_message_t packet{};
        if (ack) mavlink_msg_param_ext_ack_pack(system, component, &packet, id, raw,
            string ? MAV_PARAM_EXT_TYPE_CUSTOM : MAV_PARAM_EXT_TYPE_UINT8, result);
        else mavlink_msg_param_ext_value_pack(system, component, &packet, id, raw,
            string ? MAV_PARAM_EXT_TYPE_CUSTOM : MAV_PARAM_EXT_TYPE_UINT8, 2, 0);
        controller.processMessageForTest(packet);
    };
    value("CC_CAM_PUB_URL", "rtsp://localhost:8554/old", 0, false, 0);
    value("CC_CAM_PUB_EN", "", 0, false, 0);
    const int count = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
    QVERIFY(controller.connectStream("camera", "rtsp://localhost:8554/new"));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), count + 1, 3000);
    value("CC_CAM_PUB_URL", "rtsp://localhost:8554/old", 0, true, PARAM_ACK_IN_PROGRESS);
    QCOMPARE(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), count + 1);
    value("CC_CAM_PUB_URL", "rtsp://localhost:8554/new", 0, true, PARAM_ACK_ACCEPTED, 192);
    QCOMPARE(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), count + 1);
    value("CC_CAM_PUB_URL", "rtsp://localhost:8554/new", 0, true, PARAM_ACK_ACCEPTED);
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), count + 2, 3000);
    mavlink_message_t sent{};
    QVERIFY(mockLink()->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_PARAM_EXT_SET, sent));
    mavlink_param_ext_set_t parameter{};
    mavlink_msg_param_ext_set_decode(&sent, &parameter);
    QCOMPARE(QString::fromLatin1(parameter.param_id), QString("CC_CAM_PUB_EN"));
    QCOMPARE(parameter.target_component, uint8_t(191));
    value("CC_CAM_PUB_EN", "", 1, true, PARAM_ACK_ACCEPTED);
    QVERIFY(!controller.cameraStreamState()["pending"].toBool());
    QVERIFY(controller.disconnectStream("camera"));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), count + 3, 3000);
    value("CC_CAM_PUB_EN", "", 0, true, PARAM_ACK_ACCEPTED);
    const auto stage = [&](const char *id, const QVariant &input, uint8_t type, bool ack = false, uint8_t result = PARAM_ACK_ACCEPTED) {
        char raw[128]{};
        if (type == MAV_PARAM_EXT_TYPE_CUSTOM) {
            const auto bytes = input.toString().toUtf8();
            std::memcpy(raw, bytes.constData(), qMin(bytes.size(), qsizetype(127)));
        } else if (type == MAV_PARAM_EXT_TYPE_UINT32) {
            const uint32_t number = input.toUInt(); std::memcpy(raw, &number, sizeof(number));
        } else raw[0] = input.toUInt();
        mavlink_message_t packet{};
        if (ack) mavlink_msg_param_ext_ack_pack(system, 191, &packet, id, raw, type, result);
        else mavlink_msg_param_ext_value_pack(system, 191, &packet, id, raw, type, 7, 0);
        controller.processMessageForTest(packet);
    };
    const auto links = [&](uint32_t baud) {
        mavlink_message_t first{}, second{};
        packLinksForTest(system, 191, &first, baud, 115200, 0, 0, 0, 3, "ttyUSB0", "ttyUSB1", &second);
        controller.processMessageForTest(first); controller.processMessageForTest(second);
        mavlink_cc_serial_link_t extra{};
        extra.link_index = 2; extra.link_count = 3; extra.baudrate = 38400; extra.status = 2;
        copyField(extra.name, "Additional device"); copyField(extra.port, "ttyUSB2");
        mavlink_msg_cc_serial_link_encode(system, 191, &first, &extra);
        controller.processMessageForTest(first);
    };
    stage("CC_L0_PORT", "ttyUSB0", MAV_PARAM_EXT_TYPE_CUSTOM);
    stage("CC_L1_PORT", "ttyUSB1", MAV_PARAM_EXT_TYPE_CUSTOM);
    stage("CC_L0_BAUD", 921600, MAV_PARAM_EXT_TYPE_UINT32);
    stage("CC_L1_BAUD", 115200, MAV_PARAM_EXT_TYPE_UINT32);
    links(921600);
    const int baudCommandsBefore = mockLink()->receivedMavCommandCount(MAV_CMD_THACO_APPLY_CONFIG, 191);
    QVERIFY(controller.setLinkBaud(0, 57600));
    stage("CC_L0_BAUD", 57600, MAV_PARAM_EXT_TYPE_UINT32, true);
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavCommandCount(MAV_CMD_THACO_APPLY_CONFIG, 191), baudCommandsBefore + 1, 3000);
    mavlink_message_t commandAck{};
    mavlink_msg_command_ack_pack(system, 191, &commandAck, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_ACCEPTED, 0, 0, 0, 0);
    controller.processMessageForTest(commandAck);
    QVERIFY(controller.companionState()["pending"].toBool()); // ACK alone is not active readback
    QTest::qWait(5);
    links(921600);
    QVERIFY(controller.companionState()["pending"].toBool());
    links(57600);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.companionState()["pending"].toBool(), 1500);

    mavlink_cc_telemetry_network_t network{};
    network.config_version = 1; network.ap_status = 1;
    network.ap_prefix_length = 24; network.ap_dhcp_enabled = 1;
    copyField(network.ap_ssid, "Existing"); copyField(network.ap_ip, "192.168.10.1");
    mavlink_msg_cc_telemetry_network_encode(system, 191, &message, &network);
    controller.processMessageForTest(message);
    stage("CC_AP_SSID", "Existing", MAV_PARAM_EXT_TYPE_CUSTOM);
    stage("CC_AP_IP", "192.168.10.1/24", MAV_PARAM_EXT_TYPE_CUSTOM);
    stage("CC_AP_DHCP", 1, MAV_PARAM_EXT_TYPE_UINT8);
    const int commandsBefore = mockLink()->receivedMavCommandCount(MAV_CMD_THACO_APPLY_CONFIG, 191);
    QVERIFY(controller.applyHotspot({{"ssid", "Candidate"}, {"ipCidr", "192.168.11.1/24"}, {"dhcpEnabled", true}}));
    stage("CC_AP_SSID", "Candidate", MAV_PARAM_EXT_TYPE_CUSTOM, true);
    stage("CC_AP_IP", "192.168.10.1/24", MAV_PARAM_EXT_TYPE_CUSTOM, true, PARAM_ACK_VALUE_UNSUPPORTED);
    // A partial stage must restore the successful SSID write, never Apply it.
    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_PARAM_EXT_SET, sent), 1500);
    mavlink_msg_param_ext_set_decode(&sent, &parameter);
    QTRY_VERIFY_WITH_TIMEOUT([&]() {
        mockLink()->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_PARAM_EXT_SET, sent);
        mavlink_msg_param_ext_set_decode(&sent, &parameter);
        return QString::fromLatin1(parameter.param_id) == "CC_AP_SSID" && QString::fromLatin1(parameter.param_value) == "Existing";
    }(), 3000);
    stage("CC_AP_SSID", "Existing", MAV_PARAM_EXT_TYPE_CUSTOM, true);
    QVERIFY(!controller.hotspotState()["pending"].toBool());
    QCOMPARE(mockLink()->receivedMavCommandCount(MAV_CMD_THACO_APPLY_CONFIG, 191), commandsBefore);

    // Return a complete readback after the rollback's refresh request.
    stage("CC_AP_SSID", "Existing", MAV_PARAM_EXT_TYPE_CUSTOM);
    stage("CC_AP_IP", "192.168.10.1/24", MAV_PARAM_EXT_TYPE_CUSTOM);
    stage("CC_AP_DHCP", 1, MAV_PARAM_EXT_TYPE_UINT8);
    stage("CC_L0_PORT", "ttyUSB0", MAV_PARAM_EXT_TYPE_CUSTOM);
    stage("CC_L1_PORT", "ttyUSB1", MAV_PARAM_EXT_TYPE_CUSTOM);
    stage("CC_L0_BAUD", 57600, MAV_PARAM_EXT_TYPE_UINT32);
    stage("CC_L1_BAUD", 115200, MAV_PARAM_EXT_TYPE_UINT32);
    // A lost stage ACK must restore its uncertain write without sending Apply.
    links(57600);
    QTimer keepAlive;
    QObject::connect(&keepAlive, &QTimer::timeout, &controller, [&]() { links(57600); });
    keepAlive.start(500);
    const int timeoutSetsBefore = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
    const int timeoutCommandsBefore = mockLink()->receivedMavCommandCount(MAV_CMD_THACO_APPLY_CONFIG, 191);
    QVERIFY(controller.setLinkBaud(0, 38400));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), timeoutSetsBefore + 2, 6500);
    QVERIFY(mockLink()->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_PARAM_EXT_SET, sent));
    mavlink_msg_param_ext_set_decode(&sent, &parameter);
    QCOMPARE(QString::fromLatin1(parameter.param_id), QString("CC_L0_BAUD"));
    uint32_t restoredBaud = 0;
    std::memcpy(&restoredBaud, parameter.param_value, sizeof(restoredBaud));
    QCOMPARE(restoredBaud, uint32_t(57600));
    stage("CC_L0_BAUD", 57600, MAV_PARAM_EXT_TYPE_UINT32, true);
    keepAlive.stop();
    QVERIFY(!controller.companionState()["pending"].toBool());
    QCOMPARE(mockLink()->receivedMavCommandCount(MAV_CMD_THACO_APPLY_CONFIG, 191), timeoutCommandsBefore);
    stage("CC_AP_SSID", "Existing", MAV_PARAM_EXT_TYPE_CUSTOM);
    stage("CC_AP_IP", "192.168.10.1/24", MAV_PARAM_EXT_TYPE_CUSTOM);
    stage("CC_AP_DHCP", 1, MAV_PARAM_EXT_TYPE_UINT8);
    stage("CC_L0_PORT", "ttyUSB0", MAV_PARAM_EXT_TYPE_CUSTOM);
    stage("CC_L1_PORT", "ttyUSB1", MAV_PARAM_EXT_TYPE_CUSTOM);
    stage("CC_L0_BAUD", 57600, MAV_PARAM_EXT_TYPE_UINT32);
    stage("CC_L1_BAUD", 115200, MAV_PARAM_EXT_TYPE_UINT32);
    // The final URL operation is cancelled when its vehicle disappears.
    camera.config_version = 1;
    mavlink_msg_cc_telemetry_camera_encode(system, 191, &message, &camera);
    controller.processMessageForTest(message);
    const int beforeCancel = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
    QVERIFY(controller.connectStream("camera", "rtsp://localhost:8554/another"));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), beforeCancel + 1, 3000);
    _disconnectMockLink();
    value("CC_CAM_PUB_URL", "rtsp://localhost:8554/another", 0, true, PARAM_ACK_ACCEPTED);
    QVERIFY(!controller.cameraStreamState()["pending"].toBool());
}

void CompanionVehicleLifecycleTest::_testStreamOptionsFromCapabilityParams()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    QVERIFY(vehicle());
    const auto system = vehicle()->id();
    mavlink_cc_telemetry_camera_t cam{};
    cam.config_version = 1; cam.supported_codecs = 1;
    mavlink_message_t message{};
    mavlink_msg_cc_telemetry_camera_encode(system, 191, &message, &cam);
    controller.processMessageForTest(message);
    QVERIFY(controller.cameraStreamState()["options"].toMap()["resolution"].toStringList().isEmpty());

    char raw[128]{};
    std::strncpy(raw, " 640x480, 1280x720 ,", sizeof(raw) - 1);
    mavlink_message_t packet{};
    mavlink_msg_param_ext_value_pack(system, 191, &packet, "CC_CAM_OPT_RES", raw, MAV_PARAM_EXT_TYPE_CUSTOM, 8, 0);
    controller.processMessageForTest(packet);
    QCOMPARE(controller.cameraStreamState()["options"].toMap()["resolution"].toStringList(),
             QStringList({"640x480", "1280x720"}));
}
