#include "CcTelemetryControllerTest.h"

#include <QtTest/QSignalSpy>

#include "CcTelemetryController.h"
#include "QGCMAVLink.h"
#include "Vehicle.h"

static void packTestLinks(mavlink_message_t* msg, uint8_t sysid, uint8_t compid,
                          uint32_t fcBaud, uint32_t siyiBaud, uint32_t fcRx, uint32_t fcTx,
                          float fcRate, uint8_t fcStatus, const char* fcPort, const char* siyiPort)
{
    mavlink_msg_cc_telemetry_links_pack(
        sysid, compid, msg,
        fcRate, fcRate, fcRate, 1.0f, 0.0f, 0,
        fcRx, fcTx, fcBaud,
        fcRate, fcRate, fcRate, 1.0f, 0.0f, 0,
        fcRx, fcTx, siyiBaud,
        fcStatus, fcStatus, 1,
        fcPort, siyiPort, ""
    );
}

static void packTestCamera(mavlink_message_t* msg, uint8_t sysid, uint8_t compid,
                           uint16_t w, uint16_t h, uint16_t rot, uint16_t dw, uint16_t dh,
                           uint16_t br, uint16_t brmax, uint16_t vbv, uint8_t vfps, uint8_t dfps,
                           uint8_t pmode, uint8_t emitter, const char* camType, const char* sn,
                           const char* codec, const char* emode, const char* rtsp)
{
    mavlink_msg_cc_telemetry_camera_pack(
        sysid, compid, msg,
        w, h, dw, dh, rot, br, brmax, vbv, 8554,
        vfps, dfps, 0, 1, pmode, emitter, 2, 0, 3,
        "TestCam", camType, "USB3", sn, codec, emode, rtsp, rtsp, rtsp
    );
}

static void packTestNetwork(mavlink_message_t* msg, uint8_t sysid, uint8_t compid,
                            uint8_t ch, uint8_t n, uint8_t wmm, uint8_t wpa, uint8_t cnt,
                            uint8_t dhcp, uint8_t dns, const char* eth0_ip, const char* wlan0_ip,
                            const char* ap_ip, const char* ap_ssid, const char* ap_pass,
                            const char* ap_key, const char* ap_hw)
{
    mavlink_msg_cc_telemetry_network_pack(
        sysid, compid, msg,
        0, 0, 0, 0, 0, 0,
        ch, n, wmm, wpa, cnt, 2, 2, 2, 1, dhcp, dns, -50,
        eth0_ip, "255.255.255.0", wlan0_ip, "255.255.255.0", "MyWlan",
        ap_ip, "255.255.255.0", ap_ssid, ap_pass, ap_key, ap_hw
    );
}

UT_REGISTER_TEST(CcTelemetryControllerTest, TestLabel::Unit, TestLabel::Vehicle)
UT_REGISTER_TEST(CcTelemetryVehicleLifecycleTest, TestLabel::Integration, TestLabel::Vehicle)

void CcTelemetryControllerTest::_testGeneratedMessageDecoding()
{
    CcTelemetryController controller;
    controller.resetForTest();
    QSignalSpy linksChangedSpy(&controller, &CcTelemetryController::linksChanged);
    QSignalSpy cameraChangedSpy(&controller, &CcTelemetryController::cameraChanged);
    QSignalSpy networkChangedSpy(&controller, &CcTelemetryController::networkChanged);
    QSignalSpy visionChangedSpy(&controller, &CcTelemetryController::visionChanged);

    mavlink_message_t message{};
    packTestLinks(&message, 42, 191, 921600, 115200, 123456, 654321, 88.5F, 3, "ttyUSB0", "ttyUSB1");
    controller.processMessageForTest(message);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_baudrate")).toULongLong(), 921600ULL);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("siyi_baudrate")).toULongLong(), 115200ULL);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_bytes_rx")).toULongLong(), 123456ULL);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_bytes_tx")).toULongLong(), 654321ULL);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_port")).toString(), QStringLiteral("ttyUSB0"));
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("siyi_port")).toString(), QStringLiteral("ttyUSB1"));
    QVERIFY(controller.linksReceived());
    QCOMPARE(linksChangedSpy.count(), 1);

    packTestCamera(&message, 42, 191, 1920, 1080, 90, 640, 480, 4000, 6000, 800, 30, 15, 2, 1, "D455", "CAM-123", "h265", "cbr", "rtsp://10.0.0.2/live");
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

    packTestNetwork(&message, 42, 191, 6, 1, 1, 2, 4, 1, 1, "10.0.0.10", "192.168.1.20",
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

void CcTelemetryControllerTest::_testSourceFiltering()
{
    CcTelemetryController controller;
    controller.resetForTest();

    mavlink_message_t message{};
    mavlink_msg_heartbeat_pack(42, 1, &message, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_GENERIC, 0, 0, MAV_STATE_ACTIVE);
    controller.processMessageForTest(message);
    QCOMPARE(controller.sourceComponentId(), -1);

    packTestLinks(&message, 42, 191, 57600, 115200, 10, 20, 2.5F, 1, "primary", "camera");
    controller.processMessageForTest(message);
    QCOMPARE(controller.sourceComponentId(), 191);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_port")).toString(), QStringLiteral("primary"));

    packTestLinks(&message, 42, 192, 9600, 9600, 99, 99, 1.0F, 0, "wrong", "wrong");
    controller.processMessageForTest(message);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_port")).toString(), QStringLiteral("primary"));
}

void CcTelemetryControllerTest::_testStaleAndReset()
{
    CcTelemetryController controller;
    controller.resetForTest();
    QVERIFY(!controller.linksReceived());
    QVERIFY(!controller.linksStale());

    mavlink_message_t message{};
    packTestLinks(&message, 1, 191, 57600, 115200, 10, 20, 2.5F, 1, "primary", "camera");
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

void CcTelemetryVehicleLifecycleTest::_testDisconnectAndReconnectReset()
{
    CcTelemetryController controller;
    _connectMockLinkNoInitialConnectSequence();
    QVERIFY(vehicle());
    QVERIFY(controller.vehicleAvailable());

    mavlink_message_t message{};
    packTestLinks(&message, vehicle()->id(), 191, 921600, 115200, 10, 20, 4.5F, 3, "first", "siyi");
    emit vehicle()->mavlinkMessageReceived(message);
    QVERIFY(controller.linksReceived());
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_port")).toString(), QStringLiteral("first"));

    _disconnectMockLink();
    QVERIFY(!controller.vehicleAvailable());
    QVERIFY(!controller.linksReceived());
    QCOMPARE(controller.sourceComponentId(), -1);

    _connectMockLinkNoInitialConnectSequence();
    QVERIFY(vehicle());
    QVERIFY(controller.vehicleAvailable());
    QVERIFY(!controller.linksReceived());

    packTestLinks(&message, vehicle()->id(), 192, 57600, 57600, 30, 40, 1.5F, 1, "second", "siyi2");
    emit vehicle()->mavlinkMessageReceived(message);
    QCOMPARE(controller.sourceComponentId(), 192);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_port")).toString(), QStringLiteral("second"));
}
