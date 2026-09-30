#include "CompanionControllerTest.h"

#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtTest/QSignalSpy>
#include <memory>

#include "CompanionController.h"
#include "MAVLinkProtocol.h"
#include "MockLink.h"
#include "QGCMAVLink.h"
#include "Vehicle.h"

UT_REGISTER_TEST(CompanionControllerTest, TestLabel::Unit, TestLabel::Vehicle)
UT_REGISTER_TEST(CompanionVehicleLifecycleTest, TestLabel::Integration, TestLabel::Vehicle)

void CompanionControllerTest::_testGeneratedMessageDecoding()
{
    CompanionController controller;
    controller.resetForTest();
    QSignalSpy linksChangedSpy(&controller, &CompanionController::linksChanged);
    QSignalSpy cameraChangedSpy(&controller, &CompanionController::cameraChanged);
    QSignalSpy networkChangedSpy(&controller, &CompanionController::networkChanged);
    QSignalSpy visionChangedSpy(&controller, &CompanionController::visionChanged);

    mavlink_message_t message{};
    mavlink_msg_cc_telemetry_links_pack(42, 191, &message, 921600, 115200, 123456, 654321, 88.5F, 3, "ttyUSB0",
                                        "ttyUSB1");
    controller.processMessageForTest(message);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_baudrate")).toULongLong(), 921600ULL);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("siyi_baudrate")).toULongLong(), 115200ULL);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_bytes_rx")).toULongLong(), 123456ULL);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_bytes_tx")).toULongLong(), 654321ULL);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_bitrate_kbps")).toFloat(), 88.5F);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("link_status_flags")).toInt(), 3);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_port")).toString(), QStringLiteral("ttyUSB0"));
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("siyi_port")).toString(), QStringLiteral("ttyUSB1"));
    QVERIFY(controller.linksReceived());
    QCOMPARE(linksChangedSpy.count(), 1);

    mavlink_msg_cc_telemetry_camera_pack(42, 191, &message, 1920, 1080, 90, 640, 480, 4000, 6000, 800, 30, 15, 2, 1,
                                         "D455", "CAM-123", "h265", "cbr", "rtsp://10.0.0.2/live");
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
    QCOMPARE(controller.ccTelemetryCamera().value(QStringLiteral("rtsp_url")).toString(),
             QStringLiteral("rtsp://10.0.0.2/live"));
    QVERIFY(controller.cameraReceived());
    QCOMPARE(cameraChangedSpy.count(), 1);

    mavlink_msg_cc_telemetry_network_pack(42, 191, &message, 6, 1, 1, 2, 4, 1, 1, "10.0.0.10", "192.168.1.20",
                                          "192.168.4.1", "AgriDrone", "secret123", "WPA-PSK", "g");
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
    QVERIFY(controller.networkReceived());
    QCOMPARE(networkChangedSpy.count(), 1);

    mavlink_msg_cc_telemetry_vision_pack(42, 191, &message, 0.65F, 24.5F, 640, 640, 30, 7, 3, "weed-v2", "rgb");
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
}

void CompanionControllerTest::_testSourceFiltering()
{
    CompanionController controller;
    controller.resetForTest();

    mavlink_message_t message{};
    mavlink_msg_heartbeat_pack(42, 1, &message, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_GENERIC, 0, 0, MAV_STATE_ACTIVE);
    controller.processMessageForTest(message);
    QCOMPARE(controller.sourceComponentId(), -1);

    mavlink_msg_cc_telemetry_links_pack(42, 191, &message, 57600, 115200, 10, 20, 2.5F, 1, "primary", "camera");
    controller.processMessageForTest(message);
    QCOMPARE(controller.sourceComponentId(), 191);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_port")).toString(), QStringLiteral("primary"));

    mavlink_msg_cc_telemetry_links_pack(42, 192, &message, 9600, 9600, 99, 99, 1.0F, 0, "wrong", "wrong");
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
    mavlink_msg_cc_telemetry_links_pack(1, 191, &message, 57600, 115200, 10, 20, 2.5F, 1, "primary", "camera");
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
    mavlink_msg_cc_telemetry_links_pack(vehicle()->id(), 191, &message, 921600, 115200, 10, 20, 4.5F, 3, "first",
                                        "siyi");
    emit vehicle() -> mavlinkMessageReceived(message);
    QVERIFY(controller.linksReceived());
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_port")).toString(), QStringLiteral("first"));

    mavlink_msg_cc_telemetry_links_pack(vehicle()->id() + 1, 191, &message, 9600, 9600, 1, 1, 0.0F, 0, "wrong",
                                        "wrong");
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

    mavlink_msg_cc_telemetry_links_pack(vehicle()->id(), 191, &message, 57600, 57600, 30, 40, 1.5F, 1, "second",
                                        "siyi2");
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
    mavlink_msg_cc_telemetry_links_pack(vehicle()->id(), 191, &message, 921600, 115200, 10, 20, 4.5F, 3, "fc-current",
                                        "siyi-current");
    emit vehicle() -> mavlinkMessageReceived(message);
    QCOMPARE(controller.availablePorts(), (QStringList{QStringLiteral("fc-current"), QStringLiteral("siyi-current")}));

    controller.applyLinksConfig(QStringLiteral("fc-new"), 460800, QStringLiteral("siyi-new"), 57600);
    QCOMPARE(controller.configStatus(), QStringLiteral("Applying"));
    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_CC_TELEMETRY_LINKS) >= 1, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavCommandCount(static_cast<MAV_CMD>(44011), 191) >= 1, 3000);

    mavlink_message_t sent{};
    QVERIFY(mockLink()->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_CC_TELEMETRY_LINKS, sent));
    mavlink_cc_telemetry_links_t sentLinks{};
    mavlink_msg_cc_telemetry_links_decode(&sent, &sentLinks);
    QCOMPARE(sentLinks.fc_baudrate, 460800U);
    QCOMPARE(sentLinks.siyi_baudrate, 57600U);
    QCOMPARE(QString::fromLatin1(sentLinks.fc_port), QStringLiteral("fc-new"));
    QCOMPARE(QString::fromLatin1(sentLinks.siyi_port), QStringLiteral("siyi-new"));

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
    mavlink_msg_cc_telemetry_links_pack(vehicle()->id(), 191, &message, 460800, 57600, 10, 20, 4.5F, 3, "fc-new",
                                        "siyi-new");
    emit vehicle() -> mavlinkMessageReceived(message);
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
    mavlink_msg_cc_telemetry_links_pack(vehicle()->id(), 191, &telemetry, 921600, 115200, 10, 20, 4.5F, 3, "fc-current",
                                        "siyi-current");
    emit vehicle() -> mavlinkMessageReceived(telemetry);
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
    mavlink_msg_cc_telemetry_links_pack(vehicle()->id(), 191, &telemetry, 460800, 115200, 10, 20, 4.5F, 3, "fc-new",
                                        "siyi-current");
    emit vehicle() -> mavlinkMessageReceived(telemetry);
    QTRY_VERIFY_WITH_TIMEOUT(!tab->property("_userInteractedFc").toBool(), 1000);
    QVERIFY(tab->property("_userInteractedSiyi").toBool());
    QCOMPARE(tab->property("_selectedSiyiPort").toString(), QStringLiteral("siyi-new"));
    QVERIFY(!saveButton->property("enabled").toBool());

    QVERIFY(QMetaObject::invokeMethod(tab, "sendSiyiDraft", Q_ARG(QVariant, QVariant(QStringLiteral("siyi-new"))),
                                      Q_ARG(QVariant, QVariant(57600))));
    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavCommandCount(static_cast<MAV_CMD>(44011), 191) >= 2, 3000);
    emit vehicle() -> mavlinkMessageReceived(ack);
    mavlink_msg_cc_telemetry_links_pack(vehicle()->id(), 191, &telemetry, 460800, 57600, 10, 20, 4.5F, 3, "fc-new",
                                        "siyi-new");
    emit vehicle() -> mavlinkMessageReceived(telemetry);
    QTRY_VERIFY_WITH_TIMEOUT(saveButton->property("enabled").toBool(), 1000);

    QVERIFY(tab->setProperty("_selectedFcPort", QStringLiteral("unsaved")));
    QVERIFY(tab->setProperty("_userInteractedFc", true));
    QVERIFY(!saveButton->property("enabled").toBool());
    _disconnectMockLink();
    QTRY_VERIFY_WITH_TIMEOUT(!tab->property("_userInteractedFc").toBool(), 1000);
    QCOMPARE(tab->property("_selectedFcPort").toString(), QString());
}
