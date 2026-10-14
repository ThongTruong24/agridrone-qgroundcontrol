#include "CompanionControllerTest.h"

#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtGui/QWindow>
#include <QtTest/QSignalSpy>
#include <QtCore/QElapsedTimer>
#include <QtCore/QRegularExpression>
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

// ---- 2026-10-12 helpers: what the CC would send, injected the way the vehicle's link delivers it ----
uint8_t extTypeFor(const QVariant& v, const char* id = "")
{
    if (QByteArray(id) == "CC_STREAM_MODE") return MAV_PARAM_EXT_TYPE_UINT8;
    if (v.typeId() == QMetaType::QString) return MAV_PARAM_EXT_TYPE_CUSTOM;
    if (v.typeId() == QMetaType::Double) return MAV_PARAM_EXT_TYPE_REAL32;
    return v.toUInt() > 1 ? MAV_PARAM_EXT_TYPE_UINT32 : MAV_PARAM_EXT_TYPE_UINT8;
}

void encodeExt(const QVariant& v, uint8_t type, char raw[128])
{
    std::memset(raw, 0, 128);
    if (type == MAV_PARAM_EXT_TYPE_CUSTOM) {
        const auto bytes = v.toString().toUtf8();
        std::memcpy(raw, bytes.constData(), qMin(bytes.size(), qsizetype(127)));
    } else if (type == MAV_PARAM_EXT_TYPE_REAL32) {
        const float number = v.toFloat();
        std::memcpy(raw, &number, sizeof(number));
    } else if (type == MAV_PARAM_EXT_TYPE_UINT32) {
        const uint32_t number = v.toUInt();
        std::memcpy(raw, &number, sizeof(number));
    } else {
        raw[0] = static_cast<char>(v.toUInt());
    }
}

void primeParam(CompanionController& c, uint8_t system, const char* id, const QVariant& v)
{
    char raw[128];
    const uint8_t type = extTypeFor(v, id);
    encodeExt(v, type, raw);
    mavlink_message_t m{};
    // 2026-10-14: a count of 1 ends the PARAM_EXT read the first CC heartbeat starts (operations wait for it).
    mavlink_msg_param_ext_value_pack(system, 191, &m, id, raw, type, 1, 0);
    c.processMessageForTest(m);
}

void ackParamSet(CompanionController& c, uint8_t system, const char* id, const QVariant& v, uint8_t result = PARAM_ACK_ACCEPTED,
                 uint8_t component = 191)
{
    char raw[128];
    const uint8_t type = extTypeFor(v, id);
    encodeExt(v, type, raw);
    mavlink_message_t m{};
    mavlink_msg_param_ext_ack_pack(system, component, &m, id, raw, type, result);
    c.processMessageForTest(m);
}

uint8_t lastSetType(MockLink* link)
{
    mavlink_message_t sent{};
    mavlink_param_ext_set_t p{};
    if (link->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_PARAM_EXT_SET, sent)) mavlink_msg_param_ext_set_decode(&sent, &p);
    return p.param_type;
}

QString lastSetId(MockLink* link)
{
    mavlink_message_t sent{};
    mavlink_param_ext_set_t p{};
    if (link->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_PARAM_EXT_SET, sent)) mavlink_msg_param_ext_set_decode(&sent, &p);
    return QString::fromLatin1(p.param_id, qstrnlen(p.param_id, 16));
}

void sendLinks(CompanionController& c, uint8_t system, uint32_t fcBaud, uint32_t siyiBaud)
{
    mavlink_message_t first{}, second{};
    packLinksForTest(system, 191, &first, fcBaud, siyiBaud, 0, 0, 0, 3, "ttyUSB0", "ttyUSB1", &second);
    c.processMessageForTest(first);
    c.processMessageForTest(second);
}

// 2026-10-14: QGC sends to the CC on the link its heartbeat came in on (mockLink() in the Vehicle tests).
void sendCompanionHeartbeat(CompanionController& c, uint8_t system, LinkInterface* link = nullptr)
{
    mavlink_message_t m{};
    mavlink_msg_heartbeat_pack(system, 191, &m, MAV_TYPE_ONBOARD_CONTROLLER, MAV_AUTOPILOT_INVALID, 0, 0, MAV_STATE_ACTIVE);
    c.processMessageForTest(m, link);
}

// Both configurable links known to the controller, readback equal to telemetry.
// 2026-10-14: starts with the CC heartbeat: that, not the Vehicle, makes the CC configurable.
void primeLinksAndParams(CompanionController& c, uint8_t system, LinkInterface* link = nullptr)
{
    sendCompanionHeartbeat(c, system, link);
    sendLinks(c, system, 921600, 115200);
    primeParam(c, system, "CC_L0_PORT", QString("ttyUSB0"));
    primeParam(c, system, "CC_L1_PORT", QString("ttyUSB1"));
    primeParam(c, system, "CC_L0_BAUD", 921600u);
    primeParam(c, system, "CC_L1_BAUD", 115200u);
}

void sendNetwork(CompanionController& c, uint8_t system, const char* ssid, const char* ip, bool dhcp, bool apUp = true)
{
    mavlink_cc_telemetry_network_t net{};
    net.config_version = 1;
    net.ap_status = apUp ? 1 : 0;
    net.ap_prefix_length = 24;
    net.ap_dhcp_enabled = dhcp;
    copyField(net.ap_ssid, ssid);
    copyField(net.ap_ip, ip);
    mavlink_message_t m{};
    mavlink_msg_cc_telemetry_network_encode(system, 191, &m, &net);
    c.processMessageForTest(m);
}

void primeHotspot(CompanionController& c, uint8_t system)
{
    sendNetwork(c, system, "Existing", "192.168.10.1", true);
    primeParam(c, system, "CC_AP_SSID", QString("Existing"));
    primeParam(c, system, "CC_AP_IP", QString("192.168.10.1/24"));
    primeParam(c, system, "CC_AP_DHCP", 1u);
    primeParam(c, system, "CC_AP_PASS", QString("old-password"));
}

void sendStreamTelemetry(CompanionController& c, uint8_t system, bool camera, bool enabled, uint8_t publishStatus, uint8_t version = 1)
{
    mavlink_message_t m{};
    if (camera) {
        mavlink_cc_telemetry_camera_t t{};
        t.config_version = version; t.fpv_enabled = enabled; t.publish_status = publishStatus; t.supported_codecs = 1;
        mavlink_msg_cc_telemetry_camera_encode(system, 191, &m, &t);
    } else {
        mavlink_cc_telemetry_vision_t t{};
        t.config_version = version; t.enabled = enabled; t.publish_status = publishStatus; t.supported_codecs = 1;
        mavlink_msg_cc_telemetry_vision_encode(system, 191, &m, &t);
    }
    c.processMessageForTest(m);
}

// 2026-10-13: what setStreamMode needs: a CC heartbeat, fresh camera telemetry and the confirmed CC_STREAM_MODE.
void primeStreamMode(CompanionController& c, uint8_t system, uint8_t mode, LinkInterface* link = nullptr)
{
    sendCompanionHeartbeat(c, system, link);
    sendStreamTelemetry(c, system, true, mode != 0, mode != 0 ? 2 : 0);
    primeParam(c, system, "CC_STREAM_MODE", uint(mode));
    primeParam(c, system, "CC_CAM_RES", QString("1280x720"));
    primeParam(c, system, "CC_CAM_PUB_URL", QString("rtsp://127.0.0.1:8554/camera"));
}

int lastSetValue(MockLink* link)
{
    mavlink_message_t sent{};
    mavlink_param_ext_set_t p{};
    if (link->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_PARAM_EXT_SET, sent)) mavlink_msg_param_ext_set_decode(&sent, &p);
    return static_cast<uint8_t>(p.param_value[0]);
}

void sendCompanionText(CompanionController& c, uint8_t system, const char* text)
{
    mavlink_message_t m{};
    mavlink_msg_statustext_pack(system, 191, &m, MAV_SEVERITY_INFO, text, 0, 0);
    c.processMessageForTest(m);
}

QStringList stepTexts(const QVariantMap& operation)
{
    QStringList out;
    for (const auto& step : operation["steps"].toList()) out << step.toMap()["text"].toString();
    return out;
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

// 2026-10-14: CC messages reach the controller only through MAVLinkProtocol (the Vehicle's copy is gone), the reset
// happens when the Vehicle goes away while the CC is silent, and the CC session epoch replaces the vehicle epoch.
void CompanionVehicleLifecycleTest::_testDisconnectAndReconnectReset()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    QVERIFY(vehicle());
    QVERIFY(controller.vehicleAvailable());

    mavlink_message_t message{};
    packLinksForTest(vehicle()->id(), 191, &message, 921600, 115200, 10, 20, 4.5F, 3, "first", "siyi");
    controller.processMessageForTest(message);
    QVERIFY(controller.linksReceived());
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_port")).toString(), QStringLiteral("first"));

    packLinksForTest(vehicle()->id() + 1, 191, &message, 9600, 9600, 1, 1, 0.0F, 0, "wrong", "wrong");
    controller.processMessageForTest(message);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_port")).toString(), QStringLiteral("first"));

    _disconnectMockLink();
    QVERIFY(!controller.vehicleAvailable());
    QVERIFY(!controller.linksReceived());
    QCOMPARE(controller.sourceComponentId(), -1);

    _connectMockLinkNoInitialConnectSequence();
    QVERIFY(vehicle());
    QVERIFY(controller.vehicleAvailable());
    QVERIFY(!controller.linksReceived());

    packLinksForTest(vehicle()->id(), 191, &message, 57600, 57600, 30, 40, 1.5F, 1, "second", "siyi2");
    controller.processMessageForTest(message);
    QCOMPARE(controller.sourceComponentId(), 191);
    QCOMPARE(controller.ccTelemetryLinks().value(QStringLiteral("fc_port")).toString(), QStringLiteral("second"));

    sendCompanionHeartbeat(controller, vehicle()->id());
    const int session = controller.ccSessionEpoch();
    sendCompanionHeartbeat(controller, vehicle()->id());
    QCOMPARE(controller.ccSessionEpoch(), session);     // same CC session
    controller.setReceivedAgeForTest("heartbeat", 5000);
    sendCompanionHeartbeat(controller, vehicle()->id());
    QCOMPARE(controller.ccSessionEpoch(), session + 1); // the CC came back after > 3.5 s
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
    QVERIFY(!controller.setStreamParameter("camera", "CC_CAM_PUB_URL", "rtmp://invalid"));
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
            // 2026-10-13: one merged page: the mode switch, Cancel, and both settings groups with their addresses.
            for (const char* child : {"streamModeSwitch", "cancelStreamOperation", "videoConfig", "visionConfig",
                                      "rtspDestination", "visionRtspDestination"}) QVERIFY2(page->findChild<QObject*>(child), child);
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
        if (findItem(popup, "rtspDestination")) {
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
    function openCamera() { camera.children[0].clicked(null) } // the indicator's MouseArea (under the mode switch)
    function openCompanion() { companion.children[0].clicked(null) }
    Row {
        width: parent.width; height: 50
        CameraStatusIndicator { id: camera }
        CompanionStatusIndicator { id: companion }
    }
}
)QML", QUrl("qrc:/qml/toolbar-window-test.qml"));
    QVERIFY2(windowComponent.isReady(), qPrintable(windowComponent.errorString()));
    std::unique_ptr<QObject> window(windowComponent.create());
    QVERIFY2(window, qPrintable(windowComponent.errorString()));
    QVERIFY(QTest::qWaitForWindowExposed(qobject_cast<QWindow *>(window.get())));
    QVERIFY(QMetaObject::invokeMethod(window.get(), "openCamera"));
    QCOMPARE(window->property("opened").toString(), QString("Vehicle Stream Status"));
    QVERIFY(window->property("urlFocused").toBool());
    QVERIFY(QMetaObject::invokeMethod(window.get(), "openCompanion"));
    QCOMPARE(window->property("opened").toString(), QString("Vehicle Companion Status"));
}

// 2026-10-14: with the FC silent QGC has no Vehicle; the Companion popup must still change a link (THACO_Drone-qx3).
void CompanionControllerTest::_testLinkApplyWithoutVehicle()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    const uint8_t sys = 1;
    sendCompanionHeartbeat(controller, sys);
    QVERIFY(controller.companionState()["connected"].toBool());
    sent.clear();
    controller.requestCcParameters();
    QCOMPARE(sent.size(), 1);
    QCOMPARE(sent.last().msgid, uint32_t(MAVLINK_MSG_ID_PARAM_EXT_REQUEST_LIST));
    mavlink_param_ext_request_list_t list{};
    mavlink_msg_param_ext_request_list_decode(&sent.last(), &list);
    QCOMPARE(list.target_system, sys);
    QCOMPARE(list.target_component, uint8_t(191));
    primeLinksAndParams(controller, sys); // CC_SERIAL_LINK x2 + PARAM_EXT_VALUE for CC_L*_* (fresh)

    QVERIFY(controller.setLinkConfig(0, "ttyUSB0", 57600));
    QTRY_VERIFY_WITH_TIMEOUT(!sent.isEmpty() && sent.last().msgid == MAVLINK_MSG_ID_PARAM_EXT_SET, 2000);
    QCOMPARE(sent.last().sysid, uint8_t(MAVLinkProtocol::instance()->getSystemId()));
    ackParamSet(controller, sys, "CC_L0_BAUD", 57600u);
    QTRY_VERIFY_WITH_TIMEOUT(sent.last().msgid == MAVLINK_MSG_ID_COMMAND_LONG, 2000);
    mavlink_command_long_t cmd{};
    mavlink_msg_command_long_decode(&sent.last(), &cmd);
    QCOMPARE(cmd.command, static_cast<uint16_t>(MAV_CMD_THACO_APPLY_CONFIG));
    QCOMPARE(cmd.target_system, sys);
    QCOMPARE(cmd.target_component, static_cast<uint8_t>(191));
    QCOMPARE(cmd.param1, 1.0f);

    // 2026-10-14: only the CC's ACK for this command settles the APPLY; FAILED would end the op if either one matched.
    mavlink_message_t ack{};
    mavlink_msg_command_ack_pack(sys, 1, &ack, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_FAILED, 0, 0, 0, 0); // the FC's
    controller.processMessageForTest(ack);
    mavlink_msg_command_ack_pack(sys, 191, &ack, MAV_CMD_REQUEST_MESSAGE, MAV_RESULT_FAILED, 0, 0, 0, 0); // another command
    controller.processMessageForTest(ack);
    QVERIFY(controller.operationState("companion")["active"].toBool());

    mavlink_msg_command_ack_pack(sys, 191, &ack, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_ACCEPTED, 0, 0, 0, 0);
    controller.processMessageForTest(ack);
    QTRY_COMPARE_WITH_TIMEOUT(([&]() { sendLinks(controller, sys, 57600, 115200);
        return controller.operationState("companion")["outcome"].toString(); })(), QString("confirmed"), 4000);
}

// 2026-10-14: APPLY is not idempotent: QGC sends it once and a silent CC ends the wait once, without a resend.
void CompanionControllerTest::_testApplyNotResentWhenCompanionSilent()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    const uint8_t sys = 1;
    primeLinksAndParams(controller, sys);
    QVERIFY(controller.setLinkConfig(0, "ttyUSB0", 57600));
    QTRY_VERIFY_WITH_TIMEOUT(!sent.isEmpty() && sent.last().msgid == MAVLINK_MSG_ID_PARAM_EXT_SET, 2000);
    ackParamSet(controller, sys, "CC_L0_BAUD", 57600u);
    QTRY_VERIFY_WITH_TIMEOUT(sent.last().msgid == MAVLINK_MSG_ID_COMMAND_LONG, 2000);
    const auto commands = [&]() {
        return int(std::count_if(sent.cbegin(), sent.cend(), [](const mavlink_message_t& m) { return m.msgid == MAVLINK_MSG_ID_COMMAND_LONG; }));
    };
    QCOMPARE(commands(), 1);
    QElapsedTimer clock;
    clock.start();
    QTRY_VERIFY_WITH_TIMEOUT(clock.elapsed() > 1500, 3000); // longer than the old command queue's three 500 ms tries
    QCOMPARE(commands(), 1);
    QCOMPARE(controller.operationState("companion")["outcome"].toString(), QString("error"));
    QCOMPARE(controller.operationState("companion")["error"].toString(), QString("Companion không phản hồi"));

    // 2026-10-14: an ACK after the wait ended changes nothing.
    mavlink_message_t late{};
    mavlink_msg_command_ack_pack(sys, 191, &late, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_ACCEPTED, 0, 0, 0, 0);
    controller.processMessageForTest(late);
    QVERIFY(!controller.companionState()["pending"].toBool());
    QCOMPARE(controller.operationState("companion")["outcome"].toString(), QString("error"));
    QCOMPARE(controller.operationState("companion")["error"].toString(), QString("Companion không phản hồi"));
}

// 2026-10-14: without a Vehicle the first CC heard is the target (ADR-0001); a second system's CC is ignored.
void CompanionControllerTest::_testFirstCompanionLocksWithoutVehicle()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    sendCompanionHeartbeat(controller, 5);
    sendCompanionHeartbeat(controller, 6);
    sendLinks(controller, 6, 57600, 115200);
    QCOMPARE(controller.sourceSystemId(), 5);
    QVERIFY(!controller.linksReceived());
    QCOMPARE(sent.size(), 1); // the read the first heartbeat starts
    mavlink_param_ext_request_list_t list{};
    mavlink_msg_param_ext_request_list_decode(&sent.first(), &list);
    QCOMPARE(list.target_system, uint8_t(5));
}

// 2026-10-14: a SET or APPLY that could not be sent was shown as sent and ended later with "Companion không phản hồi".
void CompanionControllerTest::_testFailedSendEndsOperationAtOnce()
{
    CompanionController controller;
    bool commandsFail = true;
    bool allFail = false;
    controller.setSendHookForTest([&](const mavlink_message_t& m) {
        return !allFail && !(commandsFail && m.msgid == MAVLINK_MSG_ID_COMMAND_LONG);
    });
    const uint8_t sys = 1;
    primeLinksAndParams(controller, sys);
    QVERIFY(controller.setLinkConfig(0, "ttyUSB0", 57600));
    ackParamSet(controller, sys, "CC_L0_BAUD", 57600u); // the APPLY goes out now, and fails
    QCOMPARE(controller.operationState("companion")["outcome"].toString(), QString("error"));
    QCOMPARE(controller.operationState("companion")["error"].toString(), QString("Không gửi được tới Companion"));

    primeLinksAndParams(controller, sys); // answers the read the failed op started
    allFail = true;
    QVERIFY(!controller.setLinkConfig(0, "ttyUSB0", 57600)); // its first SET is not sent: nothing started
    QCOMPARE(controller.operationState("companion")["outcome"].toString(), QString("error"));
    QCOMPARE(controller.companionState()["linkError"].toString(), QString("Không gửi được tới Companion"));
}

// 2026-10-14: the CC's COMMAND_ACK (IN_PROGRESS keepalives of a long APPLY) no longer kept the telemetry watchdog alive.
void CompanionControllerTest::_testCompanionAckRestartsWatchdog()
{
    CompanionController controller;
    QVERIFY(!controller._telemetryWatchdog->isActive());
    mavlink_message_t ack{};
    mavlink_msg_command_ack_pack(1, 191, &ack, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_IN_PROGRESS, 50, 0, 0, 0);
    controller.processMessageForTest(ack);
    QVERIFY(controller._telemetryWatchdog->isActive());
}

// 2026-10-13: the stream ON/OFF half of this test went with CC_CAM_FPV_EN/PUB_EN; the readback, link and hotspot halves stay.
// 2026-10-14: renamed from _testReadbackLinkHotspotAndVehicleCancel: its tail now asserts the op outlives the Vehicle.
void CompanionVehicleLifecycleTest::_testReadbackLinkHotspotAndOpOutlivesVehicle()
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
        // 2026-10-14: the CC heartbeat, not the Vehicle, starts the CC session and its first read, on the heartbeat's link.
        const int readsAtStart = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_REQUEST_LIST);
        sendCompanionHeartbeat(*uiController, system, mockLink());
        QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_REQUEST_LIST), readsAtStart + 1, 1500);
        const int readsBefore = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_REQUEST_LIST);
        const int writesBefore = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
        uiController->processMessageForTest(message);
        QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_REQUEST_LIST), readsBefore + 1, 1500);
        uiController->processMessageForTest(message);
        QCOMPARE(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_REQUEST_LIST), readsBefore + 1);
        QCOMPARE(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), writesBefore);
    }
    mavlink_message_t sent{};
    mavlink_param_ext_set_t parameter{};
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
        else mavlink_msg_param_ext_value_pack(system, 191, &packet, id, raw, type, 1, 0); // 2026-10-14: count 1 ends each read
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
        sendCompanionHeartbeat(controller, system); // 2026-10-14: the CC stays connected through the long waits below
    };
    sendCompanionHeartbeat(controller, system, mockLink());
    stage("CC_L0_PORT", "ttyUSB0", MAV_PARAM_EXT_TYPE_CUSTOM);
    stage("CC_L1_PORT", "ttyUSB1", MAV_PARAM_EXT_TYPE_CUSTOM);
    stage("CC_L0_BAUD", 921600, MAV_PARAM_EXT_TYPE_UINT32);
    stage("CC_L1_BAUD", 115200, MAV_PARAM_EXT_TYPE_UINT32);
    links(921600);
    const int baudCommandsBefore = mockLink()->receivedMavCommandCount(MAV_CMD_THACO_APPLY_CONFIG, 191);
    QVERIFY(controller.setLinkConfig(0, "ttyUSB0", 57600));
    stage("CC_L0_BAUD", 57600, MAV_PARAM_EXT_TYPE_UINT32, true);
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavCommandCount(MAV_CMD_THACO_APPLY_CONFIG, 191), baudCommandsBefore + 1, 3000);
    mavlink_message_t commandAck{};
    mavlink_msg_command_ack_pack(system, 191, &commandAck, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_ACCEPTED, 0, 0, 0, 0);
    // 2026-10-14: QGC matches the CC's COMMAND_ACK itself; it arrives from the link like any CC message.
    mockLink()->respondWithMavlinkMessage(commandAck);
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
    QVERIFY(controller.setLinkConfig(0, "ttyUSB0", 38400));
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
    // 2026-10-14: the CC session outlives the Vehicle: with the CC heartbeat fresh, a mode operation goes on when the
    // Vehicle disappears (it used to be cancelled with it) and the CC's ACK completes it.
    primeStreamMode(controller, system, 1, mockLink());
    const int beforeCancel = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
    QVERIFY(controller.setStreamMode(2));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), beforeCancel + 1, 3000);
    _disconnectMockLink();
    QVERIFY(controller.cameraStreamState()["pending"].toBool());
    ackParamSet(controller, system, "CC_STREAM_MODE", 2u);
    QVERIFY(!controller.cameraStreamState()["pending"].toBool());
    QCOMPARE(controller.operationState("stream")["outcome"].toString(), QString("confirmed"));
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

// 2026-10-12: APPLY showed no progress; the CC's stage reports were invisible in QGC.
void CompanionVehicleLifecycleTest::_testApplyProgressFromCompanionStages()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    QVERIFY(vehicle());
    const auto system = vehicle()->id();
    primeLinksAndParams(controller, system, mockLink());
    const int sets = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
    QVERIFY(controller.setLinkConfig(1, "ttyUSB1", 57600));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), sets + 1, 3000);
    ackParamSet(controller, system, "CC_L1_BAUD", 57600u);
    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavCommandCount(MAV_CMD_THACO_APPLY_CONFIG, 191) >= 1, 3000);

    mavlink_message_t text{}, noise{}, ack{};
    mavlink_msg_statustext_pack(system, 191, &text, MAV_SEVERITY_INFO, "Links: reopening serial ports", 0, 0);
    controller.processMessageForTest(text);
    mavlink_msg_statustext_pack(system, 191, &noise, MAV_SEVERITY_WARNING, "Some CC parameters are unavailable", 0, 0);
    controller.processMessageForTest(noise);
    mavlink_msg_command_ack_pack(system, 191, &ack, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_IN_PROGRESS, 50, 0, 0, 0);
    mockLink()->respondWithMavlinkMessage(ack);

    QTRY_COMPARE_WITH_TIMEOUT(controller.operationState("companion")["progress"].toDouble(), 0.75, 2000);
    const auto texts = stepTexts(controller.operationState("companion"));
    QVERIFY(texts.contains(QStringLiteral("PARAM_EXT_SET CC_L1_BAUD=57600")));
    QVERIFY(texts.contains(QStringLiteral("Links: reopening serial ports")));
    QVERIFY(std::any_of(texts.cbegin(), texts.cend(), [](const QString& t) { return t.contains("Router mở lại cổng"); }));
    QVERIFY(!std::any_of(texts.cbegin(), texts.cend(), [](const QString& t) { return t.contains("unavailable"); }));
}

// 2026-10-12: the router restart drops the CC's ACK; QGC reported a failure although the link had changed.
void CompanionVehicleLifecycleTest::_testLinkApplyGapConfirmedByTelemetry()
{
    CompanionController controller;
    // 2026-10-14: QGC sends APPLY once itself (no command queue); the router/AP restart silences the CC instead.
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    primeLinksAndParams(controller, system, mockLink());
    const int sets = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
    QVERIFY(controller.setLinkConfig(1, "ttyUSB1", 57600));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), sets + 1, 3000);
    ackParamSet(controller, system, "CC_L1_BAUD", 57600u);
    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavCommandCount(MAV_CMD_THACO_APPLY_CONFIG, 191) >= 1, 3000);
    mavlink_message_t ack{};
    mavlink_msg_command_ack_pack(system, 191, &ack, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_IN_PROGRESS, 50, 0, 0, 0);
    mockLink()->respondWithMavlinkMessage(ack);
    // no final ACK: after the CC's silence QGC waits for telemetry instead of reporting a failure
    QTRY_VERIFY_WITH_TIMEOUT(stepTexts(controller.operationState("companion")).contains(QStringLiteral("Router đang khởi động lại, chờ telemetry")), 3000);
    QVERIFY(controller.operationState("companion")["error"].toString().isEmpty());

    QTRY_COMPARE_WITH_TIMEOUT(([&]() { sendLinks(controller, system, 921600, 57600); return controller.operationState("companion")["outcome"].toString(); })(),
                              QString("confirmed"), 4000);
    for (const auto& step : controller.operationState("companion")["steps"].toList()) QVERIFY(step.toMap()["state"].toString() != "error");
}

// 2026-10-12: with the hotspot restarting, QGC may lose its own WiFi; that is not a failure.
void CompanionVehicleLifecycleTest::_testHotspotApplyGapConfirmedByTelemetry()
{
    CompanionController controller;
    // 2026-10-14: QGC sends APPLY once itself (no command queue); the router/AP restart silences the CC instead.
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    primeLinksAndParams(controller, system, mockLink()); // makes the CC the telemetry source
    primeHotspot(controller, system);
    QVERIFY(controller.applyHotspot({{"ssid", "Candidate"}, {"ipCidr", "192.168.11.1/24"}, {"dhcpEnabled", true}}));
    const auto next = [&](const char* id, const QVariant& v, int count) {
        QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET) >= count, 3000);
        QCOMPARE(lastSetId(mockLink()), QString(id));
        ackParamSet(controller, system, id, v);
    };
    const int base = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
    next("CC_AP_SSID", QString("Candidate"), base > 0 ? base : 1);
    QTRY_VERIFY_WITH_TIMEOUT(lastSetId(mockLink()) == "CC_AP_IP", 3000);
    ackParamSet(controller, system, "CC_AP_IP", QString("192.168.11.1/24"));
    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavCommandCount(MAV_CMD_THACO_APPLY_CONFIG, 191) >= 1, 3000);
    mavlink_message_t ack{};
    mavlink_msg_command_ack_pack(system, 191, &ack, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_IN_PROGRESS, 35, 0, 0, 0);
    mockLink()->respondWithMavlinkMessage(ack);
    QTRY_VERIFY_WITH_TIMEOUT(stepTexts(controller.operationState("companion")).contains(
        QStringLiteral("Đã gửi cấu hình hotspot mới. Mất kết nối WiFi là bình thường; hãy nối lại vào SSID mới")), 3000);
    QVERIFY(controller.operationState("companion")["error"].toString().isEmpty());

    QTRY_COMPARE_WITH_TIMEOUT(([&]() { sendNetwork(controller, system, "Candidate", "192.168.11.1", true);
                                       return controller.operationState("companion")["outcome"].toString(); })(),
                              QString("confirmed"), 4000);
}

// 2026-10-12: a SET nobody answers must say so, not wait for the whole operation budget.
void CompanionVehicleLifecycleTest::_testSetWithoutAnyAckReportsNoResponse()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    primeLinksAndParams(controller, system, mockLink());
    QVERIFY(controller.setLinkConfig(1, "ttyUSB1", 57600));
    QTRY_COMPARE_WITH_TIMEOUT(controller.operationState("companion")["outcome"].toString(), QString("error"), 4000);
    QVERIFY(controller.operationState("companion")["error"].toString().contains("không phản hồi"));
    QVERIFY(stepTexts(controller.operationState("companion")).contains(QStringLiteral("(rollback) PARAM_EXT_SET CC_L1_BAUD=115200")));
}

// 2026-10-12: operation steps are visible to every GCS user; a passphrase must never be in them.
void CompanionVehicleLifecycleTest::_testSecretNeverAppearsInOperationSteps()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    primeLinksAndParams(controller, system, mockLink());
    primeHotspot(controller, system);
    QVERIFY(controller.applyHotspot({{"ssid", "Existing"}, {"ipCidr", "192.168.10.1/24"}, {"dhcpEnabled", true},
                                     {"password", "Secret-Canary-123"}}));
    QTRY_COMPARE_WITH_TIMEOUT(lastSetId(mockLink()), QString("CC_AP_PASS"), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(controller.operationState("companion")["steps"].toList().size() >= 1, true, 1000);
    ackParamSet(controller, system, "CC_AP_PASS", QString()); // a secret comes back empty
    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavCommandCount(MAV_CMD_THACO_APPLY_CONFIG, 191) >= 1, 3000);
    const auto texts = stepTexts(controller.operationState("companion"));
    QVERIFY(texts.contains(QStringLiteral("PARAM_EXT_SET CC_AP_PASS=<hidden>")));
    for (const auto& t : texts) QVERIFY2(!t.contains("Secret-Canary-123"), qPrintable(t));
}

// 2026-10-12: an address that is not rtsp://host/path is refused before anything is sent, and the popup is told why.
// 2026-10-13: the address is its own field now (no ON switch takes it), so the check moved to setStreamParameter.
void CompanionVehicleLifecycleTest::_testStreamUrlRefusesInvalidAddress()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    primeStreamMode(controller, system, 1, mockLink());
    const int base = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
    for (const char* bad : {"", "rtmp://host/live", "rtsp://host", "not a url"}) QVERIFY2(!controller.setStreamParameter("camera", "CC_CAM_PUB_URL", bad), bad);
    QCOMPARE(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), base);
    QCOMPARE(controller.operationState("stream")["outcome"].toString(), QString("error"));
    QVERIFY(stepTexts(controller.operationState("stream")).contains(QStringLiteral("Địa chỉ RTSP không hợp lệ")));
    QVERIFY(controller.setStreamParameter("camera", "CC_CAM_PUB_URL", "rtsp://localhost:8554/new"));
    QTRY_COMPARE_WITH_TIMEOUT(lastSetId(mockLink()), QString("CC_CAM_PUB_URL"), 3000);
}

// 2026-10-14: the CC refuses a publish path with %, an empty/./.. segment or characters outside A-Za-z0-9_.~/- at SET;
// the popup now refuses it locally with its own message instead of getting a generic rejection.
void CompanionVehicleLifecycleTest::_testStreamUrlPathRule()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    primeStreamMode(controller, system, 1, mockLink());
    const int base = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
    for (const char* bad : {"rtsp://drone.local:8554/camera/", "rtsp://drone.local:8554//camera", "rtsp://h/a/../camera", "rtsp://h/./camera",
                            "rtsp://h/%63amera", "rtsp://h/my cam", "rtsp://h/ca$mera"}) {
        QVERIFY2(!controller.setStreamParameter("camera", "CC_CAM_PUB_URL", bad), bad);
        if (!QByteArray(bad).contains(' ')) QVERIFY2(stepTexts(controller.operationState("stream")).contains(QStringLiteral(
            "Đường dẫn RTSP chỉ dùng A-Z a-z 0-9 _ . ~ / - (không có đoạn rỗng, . hoặc .., không có %)")), bad);
    }
    QCOMPARE(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), base);
    for (const char* good : {"rtsp://drone.local:8554/camera", "rtsp://h:8554/live/cam-1_a.b~c", "rtsp://h/camera?x=%41"}) {
        QVERIFY2(controller.setStreamParameter("camera", "CC_CAM_PUB_URL", good), good);
        QTRY_COMPARE_WITH_TIMEOUT(lastSetId(mockLink()), QString("CC_CAM_PUB_URL"), 3000);
        ackParamSet(controller, system, "CC_CAM_PUB_URL", QString(good));
        QTRY_COMPARE_WITH_TIMEOUT(controller.operationState("stream")["outcome"].toString(), QString("confirmed"), 3000);
    }
}

// 2026-10-14: the CC names the owner's reason in a "Camera: "/"Vision: " line right before FAILED; the popup shows it.
void CompanionVehicleLifecycleTest::_testStreamFailureShowsOwnerReason()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    primeStreamMode(controller, system, 1, mockLink());
    QVERIFY(controller.setStreamParameter("camera", "CC_CAM_PUB_URL", "rtsp://h/other"));
    QTRY_COMPARE_WITH_TIMEOUT(lastSetId(mockLink()), QString("CC_CAM_PUB_URL"), 3000);
    sendCompanionText(controller, system, "Camera: path /other is held by another publisher");
    ackParamSet(controller, system, "CC_CAM_PUB_URL", QString("rtsp://h/other"), PARAM_ACK_FAILED);
    QTRY_COMPARE_WITH_TIMEOUT(controller.operationState("stream")["outcome"].toString(), QString("error"), 2000);
    QCOMPARE(controller.operationState("stream")["error"].toString(), QString("Camera: path /other is held by another publisher"));
}

// 2026-10-12: popups said only "Controls require current Companion firmware and telemetry".
void CompanionVehicleLifecycleTest::_testStreamReasonTable()
{
    CompanionController controller;
    QCOMPARE(controller.streamReason("vision")["code"].toInt(), 1);
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    QCOMPARE(controller.streamReason("vision")["code"].toInt(), 2);
    sendCompanionHeartbeat(controller, system, mockLink());
    QCOMPARE(controller.streamReason("vision")["code"].toInt(), 3);
    primeParam(controller, system, "CC_CAM_RES", QString("1280x720")); // 2026-10-14: answers the read the heartbeat started
    QVERIFY(controller.streamReason("vision")["text"].toString().contains("Vision"));
    QVERIFY(controller.streamReason("camera")["text"].toString().contains("Video"));
    sendStreamTelemetry(controller, system, false, true, 0, 2);
    QCOMPARE(controller.streamReason("vision")["code"].toInt(), 5);
    sendStreamTelemetry(controller, system, false, true, 0, 1);
    QCOMPARE(controller.streamReason("vision")["code"].toInt(), 7); // no CC_VIS_RES readback yet
    controller.setReceivedAgeForTest("vision", 5000);
    QCOMPARE(controller.streamReason("vision")["code"].toInt(), 4);
    QVERIFY(controller.streamReason("vision")["text"].toString().contains("lần cuối"));
    sendStreamTelemetry(controller, system, false, true, 0, 1);
    primeParam(controller, system, "CC_VIS_RES", QString("640x480"));
    QCOMPARE(controller.streamReason("vision")["code"].toInt(), 0);
    QCOMPARE(controller.visionStreamState()["reason"].toMap()["code"].toInt(), 0);

    primeLinksAndParams(controller, system, mockLink());
    QVERIFY(controller.setLinkConfig(1, "ttyUSB1", 57600)); // an operation started from the Companion popup
    QCOMPARE(controller.streamReason("vision")["code"].toInt(), 9);
    controller.requestCcParameters();
    QCOMPARE(controller.streamReason("vision")["code"].toInt(), 6);
}

// 2026-10-12: the UART device could only be changed from the deleted Settings page.
void CompanionVehicleLifecycleTest::_testLinkPortAndBaudApplyAndConflictGuard()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    primeLinksAndParams(controller, system, mockLink()); // link 0 ttyUSB0, link 1 ttyUSB1
    QVERIFY(!controller.setLinkConfig(1, "ttyUSB0", 57600)); // that device belongs to link 0
    QVERIFY(controller.companionState()["linkError"].toString().contains("Cổng đang được dùng"));

    const int base = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
    QVERIFY(controller.setLinkConfig(1, "ttyUSB2", 57600));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), base + 1, 3000);
    QCOMPARE(lastSetId(mockLink()), QString("CC_L1_PORT"));
    ackParamSet(controller, system, "CC_L1_PORT", QString("ttyUSB2"));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), base + 2, 3000);
    QCOMPARE(lastSetId(mockLink()), QString("CC_L1_BAUD"));
    ackParamSet(controller, system, "CC_L1_BAUD", 57600u);
    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavCommandCount(MAV_CMD_THACO_APPLY_CONFIG, 191) >= 1, 3000);
    mavlink_message_t ack{};
    mavlink_msg_command_ack_pack(system, 191, &ack, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_ACCEPTED, 0, 0, 0, 0);
    mockLink()->respondWithMavlinkMessage(ack);

    // the effect needs BOTH the new device and the new baud in telemetry
    QTRY_VERIFY_WITH_TIMEOUT(([&]() { sendLinks(controller, system, 921600, 57600); return controller.operationState("companion")["active"].toBool(); })(), 2000); // baud alone is not enough
    mavlink_message_t first{}, second{};
    QTRY_COMPARE_WITH_TIMEOUT(([&]() {
        packLinksForTest(system, 191, &first, 921600, 57600, 0, 0, 0, 3, "ttyUSB0", "ttyUSB2", &second);
        controller.processMessageForTest(first); controller.processMessageForTest(second);
        return controller.operationState("companion")["outcome"].toString(); })(), QString("confirmed"), 4000);
}

// 2026-10-12: with the AP off the hotspot popup was always blocked, leaving SSH as the only recovery.
void CompanionVehicleLifecycleTest::_testHotspotApplyAllowedWhenApOff()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    primeLinksAndParams(controller, system, mockLink());
    primeHotspot(controller, system);
    sendNetwork(controller, system, "Other", "192.168.10.1", true, true); // AP up, but not what the stage says
    QVERIFY(!controller.applyHotspot({{"ssid", "Candidate"}, {"ipCidr", "192.168.11.1/24"}, {"dhcpEnabled", true}}));
    sendNetwork(controller, system, "", "192.168.10.1", true, false);     // AP off: nothing broadcast to compare with
    const int base = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
    QVERIFY(controller.applyHotspot({{"ssid", "Candidate"}, {"ipCidr", "192.168.11.1/24"}, {"dhcpEnabled", true}}));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), base + 1, 3000);
    QCOMPARE(lastSetId(mockLink()), QString("CC_AP_SSID"));
}

// 2026-10-12: Vision confidence had no control once Settings was removed. (2026-10-13: the AI branch switch is gone; the mode drives it.)
void CompanionVehicleLifecycleTest::_testVisionConfidence()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    primeLinksAndParams(controller, system, mockLink());
    sendStreamTelemetry(controller, system, false, true, 0);
    sendStreamTelemetry(controller, system, true, true, 0);
    primeParam(controller, system, "CC_VIS_CONF", 0.35);
    QVERIFY(qAbs(controller.visionStreamState()["confidence"].toDouble() - 0.35) < 0.001);

    int base = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
    QVERIFY(controller.setStreamParameter("vision", "CC_VIS_CONF", 0.5));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), base + 1, 3000);
    QCOMPARE(lastSetId(mockLink()), QString("CC_VIS_CONF"));
    QCOMPARE(lastSetType(mockLink()), uint8_t(MAV_PARAM_EXT_TYPE_REAL32));
    ackParamSet(controller, system, "CC_VIS_CONF", 0.5);
    QTRY_COMPARE_WITH_TIMEOUT(controller.operationState("stream")["outcome"].toString(), QString("confirmed"), 2000);
}

// 2026-10-13: one mode switch replaces the separate ON/OFF and AI-branch switches; the CC reports "[k/n]" steps and QGC
// reconnects its player as soon as the CC confirms the mode.
void CompanionVehicleLifecycleTest::_testStreamModeProgressAndVideoRestart()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    primeStreamMode(controller, system, 0, mockLink());
    QCOMPARE(controller.streamModeState()["mode"].toInt(), 0);
    QSignalSpy restarts(&controller, &CompanionController::videoRestarted);
    const int base = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
    QVERIFY(controller.setStreamMode(2));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), base + 1, 3000);
    QCOMPARE(lastSetId(mockLink()), QString("CC_STREAM_MODE"));
    QCOMPARE(lastSetType(mockLink()), uint8_t(MAV_PARAM_EXT_TYPE_UINT8));
    QCOMPARE(lastSetValue(mockLink()), 2);
    {
        mavlink_message_t sent{};
        mavlink_param_ext_set_t parameter{};
        QVERIFY(mockLink()->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_PARAM_EXT_SET, sent));
        mavlink_msg_param_ext_set_decode(&sent, &parameter);
        QCOMPARE(parameter.target_component, uint8_t(191));
    }
    ackParamSet(controller, system, "CC_STREAM_MODE", 2u, PARAM_ACK_ACCEPTED, 192); // not the CC: ignored
    QVERIFY(controller.streamModeState()["modeOperation"].toBool());
    auto state = controller.streamModeState();
    QCOMPARE(state["mode"].toInt(), 0);        // the switch moves only when the CC confirms
    QCOMPARE(state["pendingMode"].toInt(), 2);
    QVERIFY(state["modeOperation"].toBool());

    ackParamSet(controller, system, "CC_STREAM_MODE", 0u, PARAM_ACK_IN_PROGRESS);
    sendCompanionText(controller, system, "Camera: [3/5] MediaMTX released /camera");
    const auto operation = controller.streamModeState()["operation"].toMap();
    QVERIFY(qAbs(operation["progress"].toDouble() - 0.6) < 0.001);
    QVERIFY(stepTexts(operation).contains(QStringLiteral("Camera: [3/5] MediaMTX released /camera")));
    QCOMPARE(restarts.count(), 0);

    ackParamSet(controller, system, "CC_STREAM_MODE", 2u);
    QTRY_COMPARE_WITH_TIMEOUT(controller.streamModeState()["operation"].toMap()["outcome"].toString(), QString("confirmed"), 2000);
    state = controller.streamModeState();
    QCOMPARE(state["mode"].toInt(), 2);
    QCOMPARE(state["pendingMode"].toInt(), 2);
    QCOMPARE(restarts.count(), 1);
    QCOMPARE(restarts.takeFirst().at(0).toInt(), 2);

    QVERIFY(controller.setStreamMode(0)); // Off: the player is stopped, not restarted
    QTRY_COMPARE_WITH_TIMEOUT(lastSetValue(mockLink()), 0, 3000);
    ackParamSet(controller, system, "CC_STREAM_MODE", 0u);
    QTRY_COMPARE_WITH_TIMEOUT(restarts.count(), 1, 2000);
    QCOMPARE(restarts.takeFirst().at(0).toInt(), 0);
}

// 2026-10-13: camera/Vision work has no deadline; only 3 s of silence from the CC (300 ms in unit tests) ends the wait,
// and that is reported as "no answer", not as a failed change.
void CompanionVehicleLifecycleTest::_testStreamModeSilenceThreeSeconds()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    primeStreamMode(controller, system, 1, mockLink());
    QVERIFY(controller.setStreamMode(2));
    QTRY_COMPARE_WITH_TIMEOUT(lastSetId(mockLink()), QString("CC_STREAM_MODE"), 3000);
    bool stayedActive = true;
    int beat = 0;
    QTimer keepAlive;
    QObject::connect(&keepAlive, &QTimer::timeout, &controller, [&]() {
        // keepalives and status lines alternate; either one proves the CC is alive
        if (++beat % 2) ackParamSet(controller, system, "CC_STREAM_MODE", 1u, PARAM_ACK_IN_PROGRESS);
        else sendCompanionText(controller, system, "Camera: waiting for first frame");
        stayedActive = stayedActive && controller.streamModeState()["operation"].toMap()["active"].toBool();
    });
    keepAlive.start(100);
    QElapsedTimer clock;
    clock.start();
    QTRY_VERIFY_WITH_TIMEOUT(clock.elapsed() > 2000, 4000); // longer than every old ACK/total budget of the unit tests
    keepAlive.stop();
    QVERIFY(stayedActive);
    QTRY_COMPARE_WITH_TIMEOUT(controller.streamModeState()["operation"].toMap()["outcome"].toString(), QString("unconfirmed"), 2000);
    const auto operation = controller.streamModeState()["operation"].toMap();
    QVERIFY(stepTexts(operation).contains(QStringLiteral("CC không phản hồi")));
    QVERIFY(operation["error"].toString().isEmpty());
    QCOMPARE(controller.streamModeState()["pendingMode"].toInt(), controller.streamModeState()["mode"].toInt());
}

// 2026-10-13: Cancel sends the previous mode again; the CC fails the cancelled operation first, which must not end the new one.
void CompanionVehicleLifecycleTest::_testStreamModeCancel()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    primeStreamMode(controller, system, 1, mockLink());
    const int base = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
    QVERIFY(controller.setStreamMode(2));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), base + 1, 3000);
    ackParamSet(controller, system, "CC_STREAM_MODE", 1u, PARAM_ACK_IN_PROGRESS);
    sendCompanionText(controller, system, "Camera: [3/5] waiting: MediaMTX holds /camera");

    controller.cancelStreamOperation();
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), base + 2, 3000);
    QCOMPARE(lastSetId(mockLink()), QString("CC_STREAM_MODE"));
    QCOMPARE(lastSetValue(mockLink()), 1);
    QCOMPARE(controller.streamModeState()["mode"].toInt(), 1);
    QCOMPARE(controller.streamModeState()["pendingMode"].toInt(), 1);

    ackParamSet(controller, system, "CC_STREAM_MODE", 1u, PARAM_ACK_FAILED); // the cancelled 1 -> 2
    QVERIFY(controller.streamModeState()["operation"].toMap()["active"].toBool());
    ackParamSet(controller, system, "CC_STREAM_MODE", 1u, PARAM_ACK_IN_PROGRESS);
    ackParamSet(controller, system, "CC_STREAM_MODE", 1u);
    QTRY_COMPARE_WITH_TIMEOUT(controller.streamModeState()["operation"].toMap()["outcome"].toString(), QString("confirmed"), 2000);
    QCOMPARE(controller.streamModeState()["mode"].toInt(), 1);
    QVERIFY(stepTexts(controller.streamModeState()["operation"].toMap()).contains(QStringLiteral("Camera: [3/5] waiting: MediaMTX holds /camera")));
}

// 2026-10-13: camera and Vision run apart from flight, so they stay writable while armed; links stay locked.
void CompanionVehicleLifecycleTest::_testCameraWriteAllowedWhileArmed()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    mockLink()->setArmed(true);
    QTRY_VERIFY_WITH_TIMEOUT(vehicle()->armed(), 5000);
    primeStreamMode(controller, system, 1, mockLink());
    primeParam(controller, system, "CC_CAM_BR", 2000u);
    QCOMPARE(controller.streamReason("camera")["code"].toInt(), 0);
    const int base = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
    QVERIFY(controller.setStreamParameter("camera", "CC_CAM_BR", 4000));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), base + 1, 3000);
    QCOMPARE(lastSetId(mockLink()), QString("CC_CAM_BR"));
    ackParamSet(controller, system, "CC_CAM_BR", 4000u);
    QTRY_COMPARE_WITH_TIMEOUT(controller.operationState("stream")["outcome"].toString(), QString("confirmed"), 2000);
    QVERIFY(controller.setStreamMode(0));
    QTRY_COMPARE_WITH_TIMEOUT(lastSetId(mockLink()), QString("CC_STREAM_MODE"), 3000);
    ackParamSet(controller, system, "CC_STREAM_MODE", 0u);
    QTRY_COMPARE_WITH_TIMEOUT(controller.streamModeState()["mode"].toInt(), 0, 2000);

    primeLinksAndParams(controller, system, mockLink());
    QVERIFY(!controller.setLinkConfig(1, "ttyUSB1", 57600));
}

// 2026-10-13: a parameter of the group that is not streaming is confirmed by the CC's readback, so its ACK is enough;
// the Vision video parameters belong to the camera now and need no Vision telemetry.
void CompanionVehicleLifecycleTest::_testInactiveGroupAckOnly()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    primeStreamMode(controller, system, 1, mockLink());
    primeParam(controller, system, "CC_VIS_BR", 1000u);
    const int base = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
    QVERIFY(controller.setStreamParameter("vision", "CC_VIS_BR", 1500));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), base + 1, 3000);
    QCOMPARE(lastSetId(mockLink()), QString("CC_VIS_BR"));
    ackParamSet(controller, system, "CC_VIS_BR", 1500u);
    QTRY_COMPARE_WITH_TIMEOUT(controller.operationState("stream")["outcome"].toString(), QString("confirmed"), 2000);
}

// 2026-10-13: the CC may send one more keepalive just after the FAILED of the same parameter; it must not revive anything.
void CompanionVehicleLifecycleTest::_testStaleInProgressAfterFailedIgnored()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    primeStreamMode(controller, system, 1, mockLink());
    QVERIFY(controller.setStreamMode(2));
    QTRY_COMPARE_WITH_TIMEOUT(lastSetId(mockLink()), QString("CC_STREAM_MODE"), 3000);
    sendCompanionText(controller, system, "Camera: encoder failed to start");
    ackParamSet(controller, system, "CC_STREAM_MODE", 1u, PARAM_ACK_FAILED);
    QTRY_COMPARE_WITH_TIMEOUT(controller.operationState("stream")["outcome"].toString(), QString("error"), 2000);
    const auto steps = controller.operationState("stream")["steps"].toList().size();
    ackParamSet(controller, system, "CC_STREAM_MODE", 1u, PARAM_ACK_IN_PROGRESS);
    QCOMPARE(controller.operationState("stream")["outcome"].toString(), QString("error"));
    QCOMPARE(controller.operationState("stream")["steps"].toList().size(), steps);
    QVERIFY(!controller.streamModeState()["modeOperation"].toBool());
    QCOMPARE(controller.streamModeState()["mode"].toInt(), 1);
}

// 2026-10-13: switching the camera Off in flight asks first; Video <-> Vision does not.
void CompanionVehicleLifecycleTest::_testOffAsksWhenArmed()
{
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    mockLink()->setArmed(true);
    QTRY_VERIFY_WITH_TIMEOUT(vehicle()->armed(), 5000);
    QQmlEngine engine;
    engine.addImageProvider(QLatin1String(ColoredSvgImageProvider::ProviderId), new ColoredSvgImageProvider());
    engine.addImportPath("qrc:/qml");
    QQmlComponent component(&engine);
    component.setData(R"QML(
import QtQuick
import QGroundControl
import Custom.AgriDrone
Item {
    property var controller: CompanionController
    property var qgc: QGroundControl
    property alias modeSwitch: modeSwitch
    StreamModeSwitch { id: modeSwitch; mode: 1 }
}
)QML", QUrl("qrc:/qml/stream-mode-switch-test.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.create());
    QVERIFY2(root, qPrintable(component.errorString()));
    auto controller = qobject_cast<CompanionController*>(root->property("controller").value<QObject*>());
    QVERIFY(controller);
    primeStreamMode(*controller, system, 1, mockLink());
    QSignalSpy dialogs(root->property("qgc").value<QObject*>(), SIGNAL(showMessageDialogRequested(QObject*,QString,QString,int,QJSValue,QJSValue)));
    QVERIFY(dialogs.isValid());
    auto modeSwitch = root->property("modeSwitch").value<QObject*>();
    const int base = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
    QVERIFY(QMetaObject::invokeMethod(modeSwitch, "requested", Q_ARG(int, 0)));
    QCOMPARE(dialogs.count(), 1);
    QCOMPARE(dialogs.first().at(1).toString(), QString("Tắt camera khi đang bay?"));
    QVERIFY(!controller->streamModeState()["pending"].toBool()); // nothing sent before the user confirms
    QCOMPARE(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), base);
    QVERIFY(QMetaObject::invokeMethod(modeSwitch, "requested", Q_ARG(int, 2)));
    QCOMPARE(dialogs.count(), 1);
    QTRY_COMPARE_WITH_TIMEOUT(lastSetId(mockLink()), QString("CC_STREAM_MODE"), 3000);
    QCOMPARE(lastSetValue(mockLink()), 2);
}

// 2026-10-13: a stuck settings rebuild (e.g. MediaMTX never releases /camera) blocked the mode switch, and with it the
// only way out: a mode SET must drop the pending camera-group op (the CC ends it too) and start the mode change.
void CompanionVehicleLifecycleTest::_testModeChangeEndsStuckSettingsOp()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    primeStreamMode(controller, system, 1, mockLink());
    primeParam(controller, system, "CC_CAM_BR", 2000u);
    const int base = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
    QVERIFY(controller.setStreamParameter("camera", "CC_CAM_BR", 4000));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), base + 1, 3000);
    ackParamSet(controller, system, "CC_CAM_BR", 2000u, PARAM_ACK_IN_PROGRESS);
    QVERIFY(controller.streamModeState()["blocked"].toString().isEmpty()); // the switch stays usable: it is the way out

    QVERIFY(controller.setStreamMode(2));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), base + 2, 3000);
    QCOMPARE(lastSetId(mockLink()), QString("CC_STREAM_MODE"));
    QCOMPARE(lastSetValue(mockLink()), 2);
    auto state = controller.streamModeState();
    QVERIFY(state["modeOperation"].toBool());
    QCOMPARE(state["mode"].toInt(), 1);
    QCOMPARE(state["pendingMode"].toInt(), 2);
    QVERIFY(stepTexts(state["operation"].toMap()).contains(QStringLiteral("Bỏ thao tác đang chờ để đổi chế độ")));

    ackParamSet(controller, system, "CC_CAM_BR", 2000u, PARAM_ACK_FAILED); // the CC ends the dropped op first
    QVERIFY(controller.streamModeState()["operation"].toMap()["active"].toBool());
    ackParamSet(controller, system, "CC_STREAM_MODE", 1u, PARAM_ACK_IN_PROGRESS);
    ackParamSet(controller, system, "CC_STREAM_MODE", 2u);
    QTRY_COMPARE_WITH_TIMEOUT(controller.streamModeState()["operation"].toMap()["outcome"].toString(), QString("confirmed"), 2000);
    QCOMPARE(controller.streamModeState()["mode"].toInt(), 2);
}

// 2026-10-13: a mode SET does not cancel a Vision inference op (CC_VIS_CONF/FPS run in their own CC slot), and QGC tracks
// one op: the switch is refused with a visible reason instead of doing nothing.
void CompanionVehicleLifecycleTest::_testModeRefusedWhileVisionOpPending()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    primeStreamMode(controller, system, 1, mockLink());
    sendStreamTelemetry(controller, system, false, true, 0);
    primeParam(controller, system, "CC_VIS_CONF", 0.35);
    const int base = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
    QVERIFY(controller.setStreamParameter("vision", "CC_VIS_CONF", 0.5));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), base + 1, 3000);
    QVERIFY(!controller.streamModeState()["blocked"].toString().isEmpty());

    QVERIFY(!controller.setStreamMode(2));
    QCOMPARE(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), base + 1);
    const auto operation = controller.operationState("stream");
    QVERIFY(operation["active"].toBool());
    QVERIFY(stepTexts(operation).contains(controller.streamModeState()["blocked"].toString()));
    ackParamSet(controller, system, "CC_VIS_CONF", 0.5);
    QTRY_COMPARE_WITH_TIMEOUT(controller.operationState("stream")["outcome"].toString(), QString("confirmed"), 2000);
    QVERIFY(controller.streamModeState()["blocked"].toString().isEmpty());
}

// 2026-10-13: the player restarted only when QGC's own mode op completed, so a late ACCEPTED or a readback that moved the
// mode left it stopped while the CC streamed (black video in flight).
void CompanionVehicleLifecycleTest::_testPlayerFollowsConfirmedMode()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    primeStreamMode(controller, system, 1, mockLink());
    QSignalSpy restarts(&controller, &CompanionController::videoRestarted);
    QVERIFY(controller.setStreamMode(2));
    QTRY_COMPARE_WITH_TIMEOUT(lastSetId(mockLink()), QString("CC_STREAM_MODE"), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(controller.operationState("stream")["outcome"].toString(), QString("unconfirmed"), 2000);
    QCOMPARE(restarts.count(), 0);
    ackParamSet(controller, system, "CC_STREAM_MODE", 2u); // late ACCEPTED after "CC không phản hồi"
    QCOMPARE(restarts.count(), 1);
    QCOMPARE(restarts.takeFirst().at(0).toInt(), 2);

    primeParam(controller, system, "CC_STREAM_MODE", 0u); // readback, no op pending
    QCOMPARE(restarts.count(), 1);
    QCOMPARE(restarts.takeFirst().at(0).toInt(), 0);
    primeParam(controller, system, "CC_STREAM_MODE", 1u);
    QCOMPARE(restarts.count(), 1);
    QCOMPARE(restarts.takeFirst().at(0).toInt(), 1);
    primeParam(controller, system, "CC_STREAM_MODE", 1u); // same mode: no second restart
    QCOMPARE(restarts.count(), 0);
}

// 2026-10-13: a keepalive of the replaced mode op can arrive after the new SET; it cleared expectCancelAck, so the replaced
// op's FAILED then ended the new op with an error.
void CompanionVehicleLifecycleTest::_testReplaceIgnoresStaleKeepalive()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    primeStreamMode(controller, system, 1, mockLink());
    const int base = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
    QVERIFY(controller.setStreamMode(2));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), base + 1, 3000);
    ackParamSet(controller, system, "CC_STREAM_MODE", 1u, PARAM_ACK_IN_PROGRESS);
    QVERIFY(controller.setStreamMode(0));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), base + 2, 3000);
    ackParamSet(controller, system, "CC_STREAM_MODE", 1u, PARAM_ACK_IN_PROGRESS); // keepalive of 1 -> 2, sent before the CC got the SET
    ackParamSet(controller, system, "CC_STREAM_MODE", 1u, PARAM_ACK_FAILED);      // the CC cancels 1 -> 2
    QVERIFY(controller.streamModeState()["operation"].toMap()["active"].toBool());
    ackParamSet(controller, system, "CC_STREAM_MODE", 1u, PARAM_ACK_IN_PROGRESS);
    ackParamSet(controller, system, "CC_STREAM_MODE", 0u);
    QTRY_COMPARE_WITH_TIMEOUT(controller.streamModeState()["operation"].toMap()["outcome"].toString(), QString("confirmed"), 2000);
    QCOMPARE(controller.streamModeState()["mode"].toInt(), 0);
}

// 2026-10-13: replacing a mode op while the new one could not start (parameters loading) dropped the old op and published
// nothing; the old op must stay and the refusal must be visible.
void CompanionVehicleLifecycleTest::_testReplaceRefusedKeepsOldOp()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    primeStreamMode(controller, system, 1, mockLink());
    const int base = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
    QVERIFY(controller.setStreamMode(2));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), base + 1, 3000);
    ackParamSet(controller, system, "CC_STREAM_MODE", 1u, PARAM_ACK_IN_PROGRESS);
    controller.requestCcParameters();
    QVERIFY(controller.ccParametersLoading());
    const auto stepsBefore = controller.operationState("stream")["steps"].toList().size();

    QVERIFY(!controller.setStreamMode(0));
    QCOMPARE(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), base + 1);
    const auto state = controller.streamModeState();
    QVERIFY(state["modeOperation"].toBool());
    QCOMPARE(state["pendingMode"].toInt(), 2);
    const auto operation = state["operation"].toMap();
    QVERIFY(operation["active"].toBool());
    QCOMPARE(operation["steps"].toList().size(), stepsBefore + 1);
    QCOMPARE(operation["steps"].toList().last().toMap()["state"].toString(), QString("error"));
}

// 2026-10-14: every CC message from the Vehicle's system reached the controller twice (MAVLinkProtocol and the Vehicle).
void CompanionVehicleLifecycleTest::_testMessageProcessedOnce()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    QSignalSpy cameraChanged(&controller, &CompanionController::cameraChanged);
    mavlink_cc_telemetry_camera_t camera{};
    camera.config_version = 1;
    mavlink_message_t message{};
    mavlink_msg_cc_telemetry_camera_encode(vehicle()->id(), 191, &message, &camera);
    mockLink()->respondWithMavlinkMessage(message);
    QTRY_VERIFY_WITH_TIMEOUT(cameraChanged.count() >= 1, 2000);
    QCOMPARE(cameraChanged.count(), 1);
}

// 2026-10-14: two drones on one network: QGC locked onto the first CC it heard, not the active Vehicle's, and sent it the link change.
void CompanionVehicleLifecycleTest::_testOtherSystemCompanionGetsNoConfig()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const uint8_t own = vehicle()->id();
    const uint8_t other = own + 1;
    primeLinksAndParams(controller, other, mockLink()); // another drone's CC is heard first
    const int sets = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
    QVERIFY(!controller.setLinkConfig(1, "ttyUSB1", 57600));
    QCOMPARE(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), sets);
    QCOMPARE(controller.sourceSystemId(), -1);

    primeLinksAndParams(controller, own, mockLink()); // the Vehicle's own CC
    QCOMPARE(controller.sourceSystemId(), int(own));
    QVERIFY(controller.setLinkConfig(1, "ttyUSB1", 57600));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), sets + 1, 3000);
    mavlink_message_t set{};
    QVERIFY(mockLink()->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_PARAM_EXT_SET, set));
    QCOMPARE(mavlink_msg_param_ext_set_get_target_system(&set), own);
}

// 2026-10-14: a CC locked while there was no Vehicle stayed the target when another system's Vehicle became active.
void CompanionVehicleLifecycleTest::_testOtherVehicleResetsCompanionLock()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    const uint8_t other = 42;
    primeLinksAndParams(controller, other); // no Vehicle: the first CC heard locks
    QVERIFY(controller.setLinkConfig(1, "ttyUSB1", 57600));
    ackParamSet(controller, other, "CC_L1_BAUD", 57600u);
    mavlink_message_t ack{};
    mavlink_msg_command_ack_pack(other, 191, &ack, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_ACCEPTED, 0, 0, 0, 0);
    controller.processMessageForTest(ack); // the op now waits for telemetry
    QVERIFY(controller.companionState()["pending"].toBool());
    const int epoch = controller.ccSessionEpoch();

    _connectMockLinkNoInitialConnectSequence();
    QVERIFY(vehicle()->id() != other);
    QCOMPARE(controller.sourceSystemId(), -1);
    QVERIFY(controller.ccSessionEpoch() > epoch);
    QVERIFY(!controller.companionState()["pending"].toBool());
    QCOMPARE(controller.operationState("companion")["outcome"].toString(), QString("error"));

    sendCompanionHeartbeat(controller, other); // that CC may not lock again while this Vehicle is active
    QCOMPARE(controller.sourceSystemId(), -1);
    sendCompanionHeartbeat(controller, vehicle()->id());
    QCOMPARE(controller.sourceSystemId(), int(vehicle()->id()));
}

// 2026-10-14: a replacing mode SET that could not be sent ends inside beginToolbarOperation; the replaced op goes on and
// still ends on CC silence (the failed send had stopped its silence timer).
void CompanionVehicleLifecycleTest::_testReplaceNotSentKeepsOldOp()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    bool fail = false;
    controller.setSendHookForTest([&](const mavlink_message_t&) { return !fail; });
    primeStreamMode(controller, system, 1);
    QVERIFY(controller.setStreamMode(2));
    ackParamSet(controller, system, "CC_STREAM_MODE", 1u, PARAM_ACK_IN_PROGRESS);
    fail = true;
    QVERIFY(!controller.setStreamMode(0));
    const auto state = controller.streamModeState();
    QVERIFY(state["modeOperation"].toBool());
    QCOMPARE(state["pendingMode"].toInt(), 2);
    QVERIFY(state["operation"].toMap()["active"].toBool());
    QTRY_COMPARE_WITH_TIMEOUT(controller.operationState("stream")["outcome"].toString(), QString("unconfirmed"), 1500);
}
