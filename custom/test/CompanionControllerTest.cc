#include "CompanionControllerTest.h"

#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtGui/QWindow>
#include <QtTest/QSignalSpy>
#include <QtCore/QElapsedTimer>
#include <QtCore/QRegularExpression>
#include <QtCore/QScopeGuard>
#include <QtNetwork/QNetworkDatagram>
#include <QtNetwork/QUdpSocket>
#include <cstring>
#include <limits>
#include <memory>

#include "ColoredSvgImageProvider.h"
#include "CompanionController.h"
#include "LinkManager.h"
#include "MAVLinkProtocol.h"
#include "MockLink.h"
#include "QGCMAVLink.h"
#include "UDPLink.h"
#include "Vehicle.h"
#include "VehicleLinkManager.h"

UT_REGISTER_TEST(CompanionControllerTest, TestLabel::Unit, TestLabel::Vehicle)
UT_REGISTER_TEST(CompanionVehicleLifecycleTest, TestLabel::Integration, TestLabel::Vehicle)

namespace {
// A UDP link whose only peer is a local socket, so a test sees exactly what QGC sent on that link.
// Ported from agridrone-qgroundcontrol be5e437 (2026-10-17).
struct UdpPeerLink
{
    QUdpSocket socket;
    QList<mavlink_message_t> controls;
    SharedLinkConfigurationPtr config;

    UdpPeerLink(QObject* context, const QString& name)
    {
        QVERIFY(socket.bind(QHostAddress(QHostAddress::LocalHost), 0));
        QObject::connect(&socket, &QUdpSocket::readyRead, context, [this]() {
            while (socket.hasPendingDatagrams()) {
                const QByteArray bytes = socket.receiveDatagram().data();
                // The UDP hole punch is a partial frame; parse each datagram on its own.
                mavlink_message_t receiveBuffer{};
                mavlink_status_t receiveStatus{};
                for (const unsigned char byte : bytes) {
                    mavlink_message_t message{};
                    mavlink_status_t status{};
                    if (mavlink_frame_char_buffer(&receiveBuffer, &receiveStatus, byte, &message, &status) ==
                            MAVLINK_FRAMING_OK &&
                        message.msgid == MAVLINK_MSG_ID_CC_AI_VISION_CONTROL) {
                        controls.append(message);
                    }
                }
            }
        });
        auto udp = std::make_shared<UDPConfiguration>(name);
        udp->setDynamic(true);
        udp->setLocalPort(0);
        udp->addHost(QStringLiteral("127.0.0.1"), socket.localPort());
        config = udp;
        QVERIFY(LinkManager::instance()->createConnectedLink(config));
        QTRY_VERIFY_WITH_TIMEOUT(config->link() && config->link()->isConnected(), TestTimeout::shortMs());
    }

    ~UdpPeerLink()
    {
        if (config && config->link()) {
            LinkManager::instance()->disconnectLink(config->link());
        }
    }
};
}  // namespace

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
    if (QByteArray(id) == "CC_STREAM_MODE")
        return MAV_PARAM_EXT_TYPE_UINT8;
    if (v.typeId() == QMetaType::QString)
        return MAV_PARAM_EXT_TYPE_CUSTOM;
    if (v.typeId() == QMetaType::Double)
        return MAV_PARAM_EXT_TYPE_REAL32;
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

void ackParamSet(CompanionController& c, uint8_t system, const char* id, const QVariant& v,
                 uint8_t result = PARAM_ACK_ACCEPTED, uint8_t component = 191)
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
    if (link->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_PARAM_EXT_SET, sent))
        mavlink_msg_param_ext_set_decode(&sent, &p);
    return p.param_type;
}

QString lastSetId(MockLink* link)
{
    mavlink_message_t sent{};
    mavlink_param_ext_set_t p{};
    if (link->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_PARAM_EXT_SET, sent))
        mavlink_msg_param_ext_set_decode(&sent, &p);
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
        t.config_version = version;
        t.fpv_enabled = enabled;
        t.publish_status = publishStatus;
        t.supported_codecs = 1;
        mavlink_msg_cc_telemetry_camera_encode(system, 191, &m, &t);
    } else {
        mavlink_cc_telemetry_vision_t t{};
        t.config_version = version;
        t.enabled = enabled;
        t.publish_status = publishStatus;
        t.supported_codecs = 1;
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
    if (link->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_PARAM_EXT_SET, sent))
        mavlink_msg_param_ext_set_decode(&sent, &p);
    return static_cast<uint8_t>(p.param_value[0]);
}

void sendCompanionText(CompanionController& c, uint8_t system, const char* text)
{
    mavlink_message_t m{};
    mavlink_msg_statustext_pack(system, 191, &m, MAV_SEVERITY_INFO, text, 0, 0);
    c.processMessageForTest(m);
}

// The port link 0 reports ("" when the CC reports no link 0).
QString linkPort(const CompanionController& c)
{
    for (const auto& link : c.serialLinks()) {
        if (link.toMap()["index"].toInt() == 0) return link.toMap()["port"].toString();
    }
    return QString();
}

QStringList stepTexts(const QVariantMap& operation)
{
    QStringList out;
    for (const auto& step : operation["steps"].toList())
        out << step.toMap()["text"].toString();
    return out;
}

// ---- 2026-10-14 helpers: a CC with the FC silent (no Vehicle at all) ----
void sendSystemStatus(CompanionController& c, uint8_t system, uint8_t status)
{
    mavlink_cc_telemetry_system_t telemetry{};
    telemetry.system_status = status;
    mavlink_message_t m{};
    mavlink_msg_cc_telemetry_system_encode(system, 191, &m, &telemetry);
    c.processMessageForTest(m);
}

void sendCommandAck(CompanionController& c, uint8_t system, uint16_t command, uint8_t result, uint8_t progress)
{
    mavlink_message_t m{};
    mavlink_msg_command_ack_pack(system, 191, &m, command, result, progress, 0, 0, 0);
    c.processMessageForTest(m);
}

void sendSerialLink(CompanionController& c, uint8_t system, uint8_t index, uint8_t count, const char* name, const char* port, uint32_t baud)
{
    mavlink_cc_serial_link_t link{};
    link.link_index = index; link.link_count = count; link.status = 2; link.baudrate = baud;
    copyField(link.name, name);
    copyField(link.port, port);
    mavlink_message_t m{};
    mavlink_msg_cc_serial_link_encode(system, 191, &m, &link);
    c.processMessageForTest(m);
}

// Heartbeat, links 0/1 with their parameters, the boot port list and CC system telemetry.
void primeCompanionWithoutVehicle(CompanionController& c, uint8_t system)
{
    sendCompanionHeartbeat(c, system);
    sendSerialLink(c, system, 0, 2, "FC", "/dev/ttyAMA4", 921600);
    sendSerialLink(c, system, 1, 2, "SIYI", "/dev/ttyAMA0", 115200);
    primeParam(c, system, "CC_L0_PORT", QString("/dev/ttyAMA4"));
    primeParam(c, system, "CC_L0_BAUD", 921600u);
    primeParam(c, system, "CC_L1_PORT", QString("/dev/ttyAMA0"));
    primeParam(c, system, "CC_L1_BAUD", 115200u);
    primeParam(c, system, "CC_SER_PORTS", QString("/dev/ttyAMA4,/dev/ttyAMA0,/dev/ttyUSB0"));
    sendSystemStatus(c, system, 0);
}

// ACK ACCEPTED (same value) for every PARAM_EXT_SET in `sent`; each ACK releases the next SET, the last one the APPLY.
void ackAllSets(CompanionController& c, uint8_t system, const QList<mavlink_message_t>& sent)
{
    for (qsizetype i = 0; i < sent.size(); ++i) {
        const mavlink_message_t message = sent.at(i);
        if (message.msgid != MAVLINK_MSG_ID_PARAM_EXT_SET) continue;
        mavlink_param_ext_set_t set{};
        mavlink_msg_param_ext_set_decode(&message, &set);
        mavlink_message_t ack{};
        mavlink_msg_param_ext_ack_pack(system, 191, &ack, set.param_id, set.param_value, set.param_type, PARAM_ACK_ACCEPTED);
        c.processMessageForTest(ack);
    }
}

bool commandSent(const QList<mavlink_message_t>& sent)
{
    return QTest::qWaitFor([&]() { return !sent.isEmpty() && sent.last().msgid == MAVLINK_MSG_ID_COMMAND_LONG; }, 2000);
}

// One link (default link 0 to /dev/ttyUSB0 @ 57600): its SETs ACKed, APPLY ACCEPTED, then the CC's link telemetry confirms it.
bool runLinkApplyToAccepted(CompanionController& c, uint8_t system, QList<mavlink_message_t>& sent, uint8_t index = 0,
                            const char* name = "FC", const char* port = "/dev/ttyUSB0", uint32_t baud = 57600)
{
    if (!c.setLinkConfig(index, port, baud)) return false;
    ackAllSets(c, system, sent);
    if (!commandSent(sent)) return false;
    sendCommandAck(c, system, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_ACCEPTED, 0);
    return QTest::qWaitFor([&]() { sendSerialLink(c, system, index, 2, name, port, baud);
        return c.operationState("companion")["outcome"].toString() == QLatin1String("confirmed"); }, 2000);
}

// A second links APPLY (link 0 to /dev/ttyUSB0 @ 57600) sent: SETs ACKed, COMMAND_LONG out, no reply from the CC yet.
bool startLinkApply(CompanionController& c, QList<mavlink_message_t>& sent, uint8_t system = 1)
{
    sent.clear();
    if (!c.setLinkConfig(0, "/dev/ttyUSB0", 57600)) return false;
    ackAllSets(c, system, sent);
    return commandSent(sent);
}

int countSent(const QList<mavlink_message_t>& sent, uint32_t msgid)
{
    return int(std::count_if(sent.cbegin(), sent.cend(), [msgid](const mavlink_message_t& m) { return m.msgid == msgid; }));
}

// The AP's network telemetry, sent until the op ends (each copy newer than the reconnect); returns the op's outcome.
QString outcomeWhileAp(CompanionController& c, const char* ssid, const char* ip, bool apUp)
{
    const bool ended = QTest::qWaitFor([&]() { sendNetwork(c, 1, ssid, ip, true, apUp);
        return !c.operationState("companion")["active"].toBool(); }, 3000);
    return ended ? c.operationState("companion")["outcome"].toString() : QStringLiteral("still active");
}

// Hotspot APPLY sent; the CC's first reply IN_PROGRESS `progress` (35: AP restarting), none when negative.
bool startHotspotApply(CompanionController& c, QList<mavlink_message_t>& sent, uint8_t system = 1,
                       const QString& password = QString(), int progress = 35)
{
    c.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    primeCompanionWithoutVehicle(c, system);
    primeHotspot(c, system);
    if (!c.applyHotspot({{"ssid", "Candidate"}, {"ipCidr", "192.168.11.1/24"}, {"dhcpEnabled", true}, {"password", password}})) return false;
    ackAllSets(c, system, sent);
    if (!commandSent(sent)) return false;
    if (progress >= 0) sendCommandAck(c, system, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_IN_PROGRESS, uint8_t(progress));
    return true;
}

// ... then the CC heartbeat goes stale.
bool startHotspotApplyThenLoseCompanion(CompanionController& c, QList<mavlink_message_t>& sent, uint8_t system = 1,
                                        const QString& password = QString())
{
    if (!startHotspotApply(c, sent, system, password)) return false;
    c.setReceivedAgeForTest("heartbeat", 5000);
    return QTest::qWaitFor([&]() { return c.companionState()["waitingReconnect"].toBool(); }, 2000);
}

void readBackCandidateHotspot(CompanionController& c, uint8_t system = 1)
{
    primeParam(c, system, "CC_AP_SSID", QString("Candidate"));
    primeParam(c, system, "CC_AP_IP", QString("192.168.11.1/24"));
    primeParam(c, system, "CC_AP_DHCP", 1u);
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
    // 2026-10-14: links are read per index from serialLinks(); the FC/SIYI map (with byte counters and a fixed
    // transport type nothing showed) is gone.
    const auto links = controller.serialLinks();
    QCOMPARE(links.size(), 2);
    QCOMPARE(links.at(0).toMap()["baudrate"].toUInt(), 921600u);
    QCOMPARE(links.at(1).toMap()["baudrate"].toUInt(), 115200u);
    QCOMPARE(links.at(0).toMap()["txRate"].toFloat(), 88.5F);
    QCOMPARE(links.at(0).toMap()["status"].toInt(), 3);
    QCOMPARE(links.at(0).toMap()["port"].toString(), QStringLiteral("ttyUSB0"));
    QCOMPARE(links.at(1).toMap()["port"].toString(), QStringLiteral("ttyUSB1"));
    QCOMPARE(links.at(1).toMap()["name"].toString(), QStringLiteral("SIYI"));
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

    mavlink_msg_cc_telemetry_vision_pack(42, 191, &message, 0.65F, 24.5F, 640, 640, 30, 7, 3, "weed-v2", "rgb", 0, 0, 0,
                                         "", 0, 0);
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
    QCOMPARE(linkPort(controller), QStringLiteral("primary"));

    packLinksForTest(42, 192, &message, 9600, 9600, 99, 99, 1.0F, 0, "wrong", "wrong");
    controller.processMessageForTest(message);
    QCOMPARE(linkPort(controller), QStringLiteral("primary"));

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
    QCOMPARE(linkPort(controller), QString());
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
    QCOMPARE(linkPort(controller), QStringLiteral("first"));

    packLinksForTest(vehicle()->id() + 1, 191, &message, 9600, 9600, 1, 1, 0.0F, 0, "wrong", "wrong");
    controller.processMessageForTest(message);
    QCOMPARE(linkPort(controller), QStringLiteral("first"));

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
    QCOMPARE(linkPort(controller), QStringLiteral("second"));

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
    camera.video_width = 1280;
    camera.video_height = 720;
    camera.video_fps = 30;
    mavlink_message_t message{};
    mavlink_msg_cc_telemetry_camera_encode(42, 191, &message, &camera);
    controller.processMessageForTest(message);
    QVERIFY(!controller.cameraStreamState()["available"].toBool());
    camera.config_version = 1;
    camera.fpv_enabled = 1;
    camera.publish_status = 0;
    mavlink_msg_cc_telemetry_camera_encode(42, 191, &message, &camera);
    controller.processMessageForTest(message);
    QVERIFY(controller.cameraStreamState()["available"].toBool());
    QVERIFY(controller.cameraStreamState()["enabled"].toBool());
    QVERIFY(!controller.visionStreamState()["available"].toBool());
    mavlink_cc_telemetry_vision_t vision{};
    vision.config_version = 1;
    vision.enabled = 1;
    vision.inference_fps = 4.8F;
    vision.video_fps = 15;
    vision.input_width = 640;
    vision.input_height = 480;
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
    // 2026-10-16 (spec camera-flip-toggle): the popup's flip switch reads the CC's confirmed CC_CAM_ROT (180 = flipped;
    // a key that is missing would read 0, so 180 is what proves the value comes through)
    char rotValue[128]{};
    const uint32_t rotation = 180;
    std::memcpy(rotValue, &rotation, sizeof(rotation));
    mavlink_msg_param_ext_value_pack(42, 191, &message, "CC_CAM_ROT", rotValue, MAV_PARAM_EXT_TYPE_UINT32, 1, 0);
    controller.processMessageForTest(message);
    QCOMPARE(controller.cameraStreamState()["configured"].toMap()["rotation"].toUInt(), 180U);
    QVERIFY(!controller.setStreamParameter("vision", "CC_CAM_ROT", 0));  // a camera parameter, not a Vision one
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
                                      "rtspDestination", "visionRtspDestination", "cameraFlipSwitch", "readingNote"})
                QVERIFY2(page->findChild<QObject*>(child), child);
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
)QML",
                            QUrl("qrc:/qml/toolbar-window-test.qml"));
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

// 2026-10-19: allowed ids were hard-coded per setter; a links id sent under the camera group would have skipped the arming lock.
void CompanionControllerTest::_testParameterGateUsesCatalogAndGroup()
{
    using Values = QList<QPair<QString, QVariant>>;
    // A fresh controller per case, every id a case uses holding a confirmed value: with the gate open each call below
    // would be accepted and send a SET, so a refusal can only come from the gate. A refusal sends nothing.
    auto fresh = [](auto&& body) {
        CompanionController controller;
        QList<mavlink_message_t> sent;
        controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
        primeCompanionWithoutVehicle(controller, 1);
        sendStreamTelemetry(controller, 1, true, true, 2);
        primeParam(controller, 1, "CC_CAM_RES", QString("1280x720"));
        primeParam(controller, 1, "CC_CAM_FPS", 30u);
        primeParam(controller, 1, "CC_CAM_OPT_RES", QString("1280x720,640x480")); // read-only
        primeParam(controller, 1, "CC_STREAM_MODE", 1u);
        primeParam(controller, 1, "CC_L2_BAUD", 115200u);        // a link the catalog only knows dynamically
        primeParam(controller, 1, "CC_NOT_IN_CATALOG", 1u);      // unknown to the catalog: added as a read-only "Other" id
        sent.clear();
        return body(controller, sent);
    };
    auto refusedBegin = [&](const QString& group, const Values& values) {
        return fresh([&](CompanionController& c, QList<mavlink_message_t>& sent) {
            return !c.beginToolbarOperation(group, values) && sent.isEmpty();
        });
    };
    QVERIFY(fresh([](CompanionController& c, QList<mavlink_message_t>&) {
        return c._paramService->groupOf("CC_L2_BAUD") == QString("Telemetry");
    }));
    QVERIFY(refusedBegin("camera", {{"CC_L0_BAUD", 57600u}}));                        // links id under the camera group
    QVERIFY(refusedBegin("camera", {{"CC_L2_BAUD", 57600u}}));                        // dynamic link id under the camera group
    QVERIFY(refusedBegin("camera", {{"CC_CAM_OPT_RES", QString("1x1")}}));            // read-only
    QVERIFY(refusedBegin("camera", {{"CC_NOT_IN_CATALOG", 2u}}));                     // dynamic id outside the catalog
    QVERIFY(fresh([](CompanionController& c, QList<mavlink_message_t>& sent) {        // setStreamMode owns the mode rules
        return !c.setStreamParameter("camera", "CC_STREAM_MODE", 2) && sent.isEmpty();
    }));
    // The right group is not refused by the gate (the camera one sends its SET).
    QVERIFY(fresh([](CompanionController& c, QList<mavlink_message_t>& sent) {
        return c.setStreamParameter("camera", "CC_CAM_FPS", 15) && sent.size() == 1 &&
               sent.last().msgid == uint32_t(MAVLINK_MSG_ID_PARAM_EXT_SET);
    }));
    // ...and a dynamic link id under its own group passes the gate (the links operation sends its SET).
    QVERIFY(fresh([](CompanionController& c, QList<mavlink_message_t>& sent) {
        return c.beginToolbarOperation("links", {{"CC_L2_BAUD", 57600u}}) && sent.size() == 1 &&
               sent.last().msgid == uint32_t(MAVLINK_MSG_ID_PARAM_EXT_SET);
    }));
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
    primeParam(controller, sys, "CC_CAM_RES", QString("1280x720")); // 2026-10-19: ends the session-start LIST; a request during it is now a no-op
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

// 2026-10-19: Update/Set were refused for the whole LIST (3-6 s) although every value they need was known; and a refusal
// for a missing value asked for a second LIST, which made the CC send the list twice.
void CompanionControllerTest::_testSetWhileLoadingNeedsOnlyItsOwnValues()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    primeCompanionWithoutVehicle(controller, 1);
    controller.requestCcParameters();
    QVERIFY(controller.ccParametersLoading());
    sent.clear();
    QVERIFY(controller.setLinkConfig(1, "/dev/ttyUSB0", 57600)); // every link value is known, the LIST still runs
    QCOMPARE(sent.last().msgid, uint32_t(MAVLINK_MSG_ID_PARAM_EXT_SET));

    CompanionController other;
    QList<mavlink_message_t> otherSent;
    other.setSendHookForTest([&](const mavlink_message_t& m) { otherSent.append(m); return true; });
    sendCompanionHeartbeat(other, 1); // session start: a LIST runs
    sendSerialLink(other, 1, 0, 2, "FC", "/dev/ttyAMA4", 921600);
    sendSerialLink(other, 1, 1, 2, "SIYI", "/dev/ttyAMA0", 115200);
    sendSystemStatus(other, 1, 0);
    QVERIFY(other.ccParametersLoading());
    otherSent.clear();
    QVERIFY(!other.setLinkConfig(1, "/dev/ttyUSB0", 57600)); // link values not read yet
    QVERIFY(otherSent.isEmpty());                              // the running LIST brings them: no second LIST
    primeParam(other, 1, "CC_CAM_RES", QString("1280x720"));   // this helper ends the LIST
    QVERIFY(!other.ccParametersLoading());
    QVERIFY(!other.setLinkConfig(1, "/dev/ttyUSB0", 57600));
    QCOMPARE(otherSent.size(), 1);                             // not loading: exactly one LIST
    QCOMPARE(otherSent.last().msgid, uint32_t(MAVLINK_MSG_ID_PARAM_EXT_REQUEST_LIST));
}

// 2026-10-19: opening a popup in the first seconds (ensureReadback) asked for a second LIST while the first ran, so the CC
// sent the whole list twice on the SIYI UART.
void CompanionControllerTest::_testRequestWhileLoadingSendsNoSecondList()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    sendCompanionHeartbeat(controller, 1); // session start: the first LIST runs
    QVERIFY(controller.ccParametersLoading());
    sent.clear();
    controller.requestCcParameters();
    QVERIFY(sent.isEmpty());
}

// 2026-10-19: the popup stayed locked until the list had read its values, and values of the previous session still
// counted as confirmed because only the 3.5 s telemetry watchdog (which a heartbeat-only CC never starts) cleared them.
void CompanionControllerTest::_testStreamSetNeedsNoKnownValueAndNewSessionForgetsOldOnes()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    primeStreamMode(controller, 1, 1);
    QVERIFY(controller._paramService->confirmedValue("CC_CAM_RES").isValid());
    controller.setReceivedAgeForTest("heartbeat", 5000); // the CC was silent for more than 3.5 s
    sendCompanionHeartbeat(controller, 1);               // a new session
    QVERIFY(!controller._paramService->confirmedValue("CC_CAM_RES").isValid());
    sent.clear();
    QVERIFY(controller.setStreamParameter("camera", "CC_CAM_FPS", 15)); // no known value needed
    QCOMPARE(sent.size(), 1);
    QCOMPARE(sent.last().msgid, uint32_t(MAVLINK_MSG_ID_PARAM_EXT_SET));
}

// 2026-10-19: with no known previous mode a cancel read an invalid QVariant as 0 and would have switched the camera Off.
void CompanionControllerTest::_testCancelStreamOperationWithoutKnownModeSendsNothing()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    sendCompanionHeartbeat(controller, 1);
    sendStreamTelemetry(controller, 1, true, true, 2);
    primeParam(controller, 1, "CC_CAM_RES", QString("1280x720")); // no CC_STREAM_MODE: the old mode is unknown
    QVERIFY(controller.setStreamMode(2));
    QVERIFY(controller.modeOperationPending());
    sent.clear();
    controller.cancelStreamOperation();
    QVERIFY(sent.isEmpty());
}

// 2026-10-19: a new CC session that started while the old session's LIST was still loading got no LIST (requestParameters returns while loading).
void CompanionControllerTest::_testNewSessionWhileOldListLoadsSendsOneList()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    sendCompanionHeartbeat(controller, 1); // session 1: its LIST runs and is not ended
    QVERIFY(controller.ccParametersLoading());
    sent.clear();
    controller.setReceivedAgeForTest("heartbeat", 5000);
    sendCompanionHeartbeat(controller, 1); // session 2
    int lists = 0;
    for (const auto& m : sent)
        if (m.msgid == MAVLINK_MSG_ID_PARAM_EXT_REQUEST_LIST) ++lists;
    QCOMPARE(lists, 1);
}

// 2026-10-19: the READ burst 4 s after a LIST shared the UART with a SET's ACK and keepalives.
void CompanionControllerTest::_testBackgroundReadsWaitForARunningOperation()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    primeStreamMode(controller, 1, 1); // most catalog values are still unread
    QVERIFY(controller.setStreamParameter("camera", "CC_CAM_FPS", 15));
    sent.clear();
    controller._paramService->readMissing();
    QVERIFY(sent.isEmpty());
    ackParamSet(controller, 1, "CC_CAM_FPS", 15u); // the operation ends
    controller._paramService->readMissing();
    QVERIFY(!sent.isEmpty());
    QCOMPARE(sent.last().msgid, uint32_t(MAVLINK_MSG_ID_PARAM_EXT_REQUEST_READ));
}

// 2026-10-19: the background-read hold also blocked the READ an APPLY's reconnect readback waits for.
void CompanionControllerTest::_testReconnectReadbackIsNotHeld()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    primeCompanionWithoutVehicle(controller, 1);
    QVERIFY(controller.setLinkConfig(1, "/dev/ttyUSB0", 57600));
    ackAllSets(controller, 1, sent);
    QVERIFY(commandSent(sent));
    sendCommandAck(controller, 1, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_IN_PROGRESS, 50);
    controller.setReceivedAgeForTest("heartbeat", 5000);
    QTRY_VERIFY_WITH_TIMEOUT(controller.companionState()["waitingReconnect"].toBool(), 2000);
    sendCompanionHeartbeat(controller, 1); // session start: readback requested, the candidate values forgotten
    QVERIFY(controller._toolbarOperation && controller._toolbarOperation->readbackRequested);
    primeParam(controller, 1, "CC_SER_PORTS", QString("/dev/ttyAMA4,/dev/ttyAMA0,/dev/ttyUSB0")); // the session LIST ends, the CC skipped the link values
    QVERIFY(controller._toolbarOperation); // still waiting for the readback
    sent.clear();
    controller._paramService->readMissing();
    bool readLinkValue = false;
    for (const auto& m : sent) {
        if (m.msgid != MAVLINK_MSG_ID_PARAM_EXT_REQUEST_READ) continue;
        char id[17] = {};
        mavlink_msg_param_ext_request_read_get_param_id(&m, id);
        readLinkValue |= QString::fromLatin1(id).startsWith(QLatin1String("CC_L1_"));
    }
    QVERIFY(readLinkValue);
}

// 2026-10-19: a LIST may run while a SET is in flight; its older VALUE must neither fail the operation nor stay for good.
void CompanionControllerTest::_testStaleValueAroundAckDoesNotBreakAnOperation()
{
    CompanionController controller;
    controller.setSendHookForTest([&](const mavlink_message_t&) { return true; });
    primeStreamMode(controller, 1, 1);
    primeParam(controller, 1, "CC_CAM_FPS", 30u);
    QVERIFY(controller.setStreamParameter("camera", "CC_CAM_FPS", 15));
    primeParam(controller, 1, "CC_CAM_FPS", 30u); // a LIST value from before the SET, ahead of the ACK
    ackParamSet(controller, 1, "CC_CAM_FPS", 15u);
    QVERIFY(!controller.cameraStreamState()["pending"].toBool());
    QCOMPARE(controller.operationState("stream")["outcome"].toString(), QString("confirmed")); // the stale VALUE did not fail it
    QVERIFY(controller.operationState("stream")["error"].toString().isEmpty());
    QCOMPARE(controller.cameraStreamState()["configured"].toMap()["fps"].toUInt(), 15U);
    primeParam(controller, 1, "CC_CAM_FPS", 30u); // an older VALUE after the ACK (reordered link)
    QCOMPARE(controller.cameraStreamState()["configured"].toMap()["fps"].toUInt(), 30U); // shown until the CC's next VALUE
    primeParam(controller, 1, "CC_CAM_FPS", 15u); // the CC's VALUE after completion corrects it
    QCOMPARE(controller.cameraStreamState()["configured"].toMap()["fps"].toUInt(), 15U);
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

// 2026-10-14: QGC must not guess "not armed" without a Vehicle; the CC's lock bit decides.
void CompanionControllerTest::_testLockFromCompanionBitWithoutVehicle()
{
    CompanionController controller;
    controller.setSendHookForTest([](const mavlink_message_t&) { return true; });
    primeCompanionWithoutVehicle(controller, 1);
    QVERIFY(!controller.companionState()["locked"].toBool());
    QVERIFY(controller.setLinkConfig(0, "ttyUSB0", 57600)); // unlocked: the same request starts
    ackParamSet(controller, 1, "CC_L0_PORT", QString("ttyUSB0"), PARAM_ACK_FAILED); // ends it (first SET: nothing to roll back)
    primeParam(controller, 1, "CC_L0_PORT", QString("/dev/ttyAMA4"));               // answers the re-read the failure started
    sendSystemStatus(controller, 1, CC_SYS_STATUS_CONFIG_LOCKED);
    QVERIFY(controller.companionState()["locked"].toBool());
    QVERIFY(controller.hotspotState()["locked"].toBool());
    QCOMPARE(controller.companionState()["lockReason"].toString(), QString("Companion đang khóa cấu hình (máy bay ARM)"));
    QVERIFY(!controller.setLinkConfig(0, "ttyUSB0", 57600));
}

// 2026-10-14: the CC names the reason in a STATUSTEXT before PARAM_ACK_FAILED; QGC shows it, not a guess.
void CompanionControllerTest::_testRefusalShowsCompanionReason()
{
    CompanionController controller;
    controller.setSendHookForTest([](const mavlink_message_t&) { return true; });
    primeCompanionWithoutVehicle(controller, 1);
    QVERIFY(controller.setLinkConfig(0, "ttyUSB9", 115200));
    sendCompanionText(controller, 1, "Links: CC_L0_PORT: not present at boot");
    ackParamSet(controller, 1, "CC_L0_PORT", QString("ttyUSB9"), PARAM_ACK_FAILED);
    QTRY_COMPARE_WITH_TIMEOUT(controller.operationState("companion")["error"].toString(),
                              QString("Links: CC_L0_PORT: not present at boot"), 2000);

    // 2026-10-14: no STATUSTEXT before the FAILED: QGC says so instead of inventing a reason.
    primeParam(controller, 1, "CC_L0_PORT", QString("/dev/ttyAMA4")); // answers the re-read the failure started
    QVERIFY(controller.setLinkConfig(0, "ttyUSB9", 115200));
    ackParamSet(controller, 1, "CC_L0_PORT", QString("ttyUSB9"), PARAM_ACK_FAILED);
    QCOMPARE(controller.operationState("companion")["error"].toString(), QString("CC từ chối (không có lý do)"));

    // 2026-10-14: an ACCEPTED ACK with another value read "CC từ chối"; the CC did not refuse anything.
    primeParam(controller, 1, "CC_L0_PORT", QString("/dev/ttyAMA4"));
    QVERIFY(controller.setLinkConfig(0, "ttyUSB9", 115200));
    ackParamSet(controller, 1, "CC_L0_PORT", QString("ttyUSB8"));
    QCOMPARE(controller.operationState("companion")["error"].toString(), QString("ACK khác giá trị đã gửi"));
}

// 2026-10-14: a hotspot APPLY restarts the AP QGC may be using; losing the CC then is not a failure.
void CompanionControllerTest::_testApplyWaitsThroughHeartbeatLossThenReadsBack()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    primeCompanionWithoutVehicle(controller, 1);
    primeHotspot(controller, 1);
    QVERIFY(controller.applyHotspot({{"ssid", "Candidate"}, {"ipCidr", "192.168.11.1/24"}, {"dhcpEnabled", true}}));
    ackAllSets(controller, 1, sent);
    QTRY_VERIFY_WITH_TIMEOUT(sent.last().msgid == MAVLINK_MSG_ID_COMMAND_LONG, 2000);
    sendCommandAck(controller, 1, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_IN_PROGRESS, 35);
    controller.setReceivedAgeForTest("heartbeat", 5000); // CC gone
    QTRY_VERIFY_WITH_TIMEOUT(stepTexts(controller.operationState("companion")).contains(QStringLiteral("Chưa xác nhận, đang chờ kết nối lại")), 2000);
    QVERIFY(controller.companionState()["waitingReconnect"].toBool());
    QVERIFY(controller.operationState("companion")["active"].toBool());
    // No deadline while waiting: neither the operation budget nor the silence timer runs.
    QVERIFY(!controller._toolbarOperationTimer->isActive());
    QVERIFY(!controller._setAckTimer->isActive());
    sent.clear();
    sendCompanionHeartbeat(controller, 1); // CC back
    QTRY_VERIFY_WITH_TIMEOUT(std::any_of(sent.cbegin(), sent.cend(), [](const auto& m) { return m.msgid == MAVLINK_MSG_ID_PARAM_EXT_REQUEST_LIST; }), 2000);
    readBackCandidateHotspot(controller);
    // 2026-10-15: this readback said "confirmed", but the CC keeps a failed hotspot candidate active and staged, so it
    // matches either way (asserted "confirmed" here before). Only the AP's telemetry after the reconnect confirms it.
    QVERIFY(controller.operationState("companion")["active"].toBool());
    QVERIFY(controller._toolbarOperationTimer->isActive()); // a bounded window for the AP to show the candidate
    controller._clearTelemetryState(); // lost again inside it: no deadline while the CC is away
    QVERIFY(controller.operationState("companion")["active"].toBool());
    QVERIFY(!controller._toolbarOperationTimer->isActive());
    sendCompanionHeartbeat(controller, 1);
    QCOMPARE(outcomeWhileAp(controller, "Candidate", "192.168.11.1", true), QString("confirmed"));
    QVERIFY(controller.hotspotState()["unsaved"].toBool());
    QVERIFY(controller.saveCompanionGroup("hotspot"));
}

// 2026-10-15: the CC writes a hotspot candidate to its active file before verifying it and keeps it when the AP fails; the
// matching readback after a reconnect said "confirmed" and SAVE persisted the failed AP config.
void CompanionControllerTest::_testHotspotReadbackAloneDoesNotConfirm()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    QVERIFY(startHotspotApplyThenLoseCompanion(controller, sent));
    sendCompanionHeartbeat(controller, 1);
    readBackCandidateHotspot(controller); // the stage equals the candidate whether the AP came up or not
    QCOMPARE(outcomeWhileAp(controller, "Existing", "192.168.10.1", false), QString("unconfirmed")); // AP down, old config
    QVERIFY(stepTexts(controller.operationState("companion")).contains(QStringLiteral("Chưa xác nhận: AP chưa phát cấu hình mới")));
    QVERIFY(!controller.hotspotState()["unsaved"].toBool());
    QVERIFY(!controller.saveCompanionGroup("hotspot"));
}

// 2026-10-15: back after the loss with the AP up on another config, the candidate did not come up; the readback said "confirmed".
void CompanionControllerTest::_testHotspotOtherConfigAfterReconnectFails()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    QVERIFY(startHotspotApplyThenLoseCompanion(controller, sent));
    sendCompanionHeartbeat(controller, 1);
    readBackCandidateHotspot(controller);
    QCOMPARE(outcomeWhileAp(controller, "Existing", "192.168.10.1", true), QString("error"));
    QCOMPARE(controller.operationState("companion")["error"].toString(), QString("Không áp dụng được"));
    QVERIFY(!controller.saveCompanionGroup("hotspot"));
}

// 2026-10-14: spec test 15 names links too: a links APPLY may change the baud of the very link QGC uses.
void CompanionControllerTest::_testLinkApplyWaitsThroughHeartbeatLossThenReadsBack()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    primeCompanionWithoutVehicle(controller, 1);
    QVERIFY(controller.setLinkConfig(1, "/dev/ttyUSB0", 57600));
    ackAllSets(controller, 1, sent);
    QVERIFY(commandSent(sent));
    sendCommandAck(controller, 1, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_IN_PROGRESS, 50); // router reopening the UARTs
    controller.setReceivedAgeForTest("heartbeat", 5000);
    QTRY_VERIFY_WITH_TIMEOUT(controller.companionState()["waitingReconnect"].toBool(), 2000);
    QVERIFY(controller.operationState("companion")["error"].toString().isEmpty());
    sendCompanionHeartbeat(controller, 1);
    primeParam(controller, 1, "CC_L1_PORT", QString("/dev/ttyUSB0"));
    primeParam(controller, 1, "CC_L1_BAUD", 57600u);
    QTRY_COMPARE_WITH_TIMEOUT(controller.operationState("companion")["outcome"].toString(), QString("confirmed"), 3000);
    QVERIFY(controller.companionState()["unsaved"].toBool());
}

// 2026-10-14: an ACCEPTED APPLY proves receipt as well as IN_PROGRESS; its readback after a reconnect must confirm it.
void CompanionControllerTest::_testAcceptedApplyReadbackConfirms()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    primeCompanionWithoutVehicle(controller, 1);
    QVERIFY(controller.setLinkConfig(1, "/dev/ttyUSB0", 57600));
    ackAllSets(controller, 1, sent);
    QVERIFY(commandSent(sent));
    sendCommandAck(controller, 1, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_ACCEPTED, 0); // no IN_PROGRESS before it
    controller._clearTelemetryState(); // the CC goes quiet before its link telemetry confirms
    QVERIFY(controller.companionState()["waitingReconnect"].toBool());
    sendCompanionHeartbeat(controller, 1);
    primeParam(controller, 1, "CC_L1_PORT", QString("/dev/ttyUSB0"));
    primeParam(controller, 1, "CC_L1_BAUD", 57600u);
    QTRY_COMPARE_WITH_TIMEOUT(controller.operationState("companion")["outcome"].toString(), QString("confirmed"), 3000);
}

// 2026-10-14: the CC's first IN_PROGRESS proves it has the APPLY; its silence after that (the router reopening the UART
// QGC uses drops the stage-50 report) ended as "Companion không phản hồi", and the applied change could never be saved.
void CompanionControllerTest::_testApplySilenceAfterInProgressWaitsForTelemetry()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    primeCompanionWithoutVehicle(controller, 1);
    QVERIFY(controller.setLinkConfig(0, "/dev/ttyUSB0", 57600));
    ackAllSets(controller, 1, sent);
    QVERIFY(commandSent(sent));
    sendCommandAck(controller, 1, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_IN_PROGRESS, 20); // checking the config
    QTRY_VERIFY_WITH_TIMEOUT(stepTexts(controller.operationState("companion")).contains(QStringLiteral("Router đang khởi động lại, chờ telemetry")), 2000);
    QVERIFY(controller.operationState("companion")["active"].toBool());
    QTRY_COMPARE_WITH_TIMEOUT(([&]() { sendSerialLink(controller, 1, 0, 2, "FC", "/dev/ttyUSB0", 57600);
        return controller.operationState("companion")["outcome"].toString(); })(), QString("confirmed"), 2000);
    QVERIFY(controller.companionState()["unsaved"].toBool());
}

// 2026-10-14: an APPLY lost on the wire leaves the CC's staged values equal to the candidate; the readback after the
// reconnect then said "confirmed" and offered Lưu while the CC ran the old config.
void CompanionControllerTest::_testApplyWithoutReceiptReadbackIsUnconfirmed()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    QVERIFY(startHotspotApply(controller, sent, 1, QString(), -1)); // no IN_PROGRESS: nothing proves the CC got it
    controller._clearTelemetryState(); // the CC goes quiet before the 3 s silence (what the telemetry watchdog does)
    QVERIFY(controller.companionState()["waitingReconnect"].toBool());
    sendCompanionHeartbeat(controller, 1);
    readBackCandidateHotspot(controller);
    QTRY_COMPARE_WITH_TIMEOUT(controller.operationState("companion")["outcome"].toString(), QString("unconfirmed"), 3000);
    // 2026-10-15: the hotspot readback no longer decides (it was the step "Chưa xác nhận" here); the AP window ends it.
    QVERIFY(stepTexts(controller.operationState("companion")).contains(QStringLiteral("Chưa xác nhận: AP chưa phát cấu hình mới")));
    QVERIFY(!controller.hotspotState()["unsaved"].toBool());
    QVERIFY(!controller.saveCompanionGroup("hotspot"));
}

// 2026-10-14: a failed links APPLY leaves the CC's active links as they were, so the change applied before it is still
// unsaved; QGC hid Lưu and a reboot reverted it silently.
void CompanionControllerTest::_testFailedLinkApplyKeepsEarlierUnsaved()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    primeCompanionWithoutVehicle(controller, 1);
    QVERIFY(runLinkApplyToAccepted(controller, 1, sent, 1, "SIYI", "/dev/ttyAMA0", 57600));
    QVERIFY(controller.companionState()["unsaved"].toBool());

    sent.clear();
    QVERIFY(controller.setLinkConfig(0, "/dev/ttyUSB0", 57600));
    ackAllSets(controller, 1, sent);
    QVERIFY(commandSent(sent));
    sendCommandAck(controller, 1, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_FAILED, 0);
    QCOMPARE(controller.operationState("companion")["outcome"].toString(), QString("error"));
    QVERIFY(controller.companionState()["unsaved"].toBool());
    primeParam(controller, 1, "CC_L0_PORT", QString("/dev/ttyAMA4")); // the CC resynced its stage; answers the re-read
    primeParam(controller, 1, "CC_L0_BAUD", 921600u);

    sendCompanionHeartbeat(controller, 1);
    QVERIFY(controller.saveCompanionGroup("links"));
    mavlink_command_long_t cmd{};
    mavlink_msg_command_long_decode(&sent.last(), &cmd);
    QCOMPARE(cmd.command, static_cast<uint16_t>(MAV_CMD_THACO_SAVE_DEFAULT_CONFIG));
    QCOMPARE(cmd.param1, 1.0f);
}

// 2026-10-14: after a confirmed links APPLY, a later one cancelled while waiting still offered Lưu; SAVE would persist the
// candidate the CC may be running (a bad baud surviving a reboot). Only a confirmed APPLY offers it again.
// (Replaces the cancel half of _testFailedLinkApplyKeepsEarlierUnsaved, which asserted Lưu stayed offered.)
void CompanionControllerTest::_testCancelledLinkApplyBlocksSave()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    primeCompanionWithoutVehicle(controller, 1);
    QVERIFY(runLinkApplyToAccepted(controller, 1, sent, 1, "SIYI", "/dev/ttyAMA0", 57600));
    QVERIFY(controller.companionState()["unsaved"].toBool());

    QVERIFY(startLinkApply(controller, sent));
    sendCommandAck(controller, 1, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_IN_PROGRESS, 50);
    controller.setReceivedAgeForTest("heartbeat", 5000);
    QTRY_VERIFY_WITH_TIMEOUT(controller.companionState()["waitingReconnect"].toBool(), 2000);
    controller.cancelCompanionWait();
    QCOMPARE(controller.operationState("companion")["outcome"].toString(), QString("unconfirmed"));
    sendCompanionHeartbeat(controller, 1);
    QVERIFY(!controller.companionState()["unsaved"].toBool());
    QVERIFY(!controller.saveCompanionGroup("links"));

    sendSerialLink(controller, 1, 0, 2, "FC", "/dev/ttyAMA4", 921600); // the CC's link telemetry and parameters after the loss
    sendSerialLink(controller, 1, 1, 2, "SIYI", "/dev/ttyAMA0", 57600);
    primeParam(controller, 1, "CC_L0_PORT", QString("/dev/ttyUSB0")); // its stage still holds the cancelled candidate
    primeParam(controller, 1, "CC_L0_BAUD", 57600u);
    primeParam(controller, 1, "CC_L1_PORT", QString("/dev/ttyAMA0"));
    primeParam(controller, 1, "CC_L1_BAUD", 57600u);
    sent.clear();
    // 2026-10-15: the natural retry, the cancelled candidate again (it sent nothing, so this test used another value).
    QVERIFY(runLinkApplyToAccepted(controller, 1, sent));
    QVERIFY(controller.companionState()["unsaved"].toBool());
    QVERIFY(controller.saveCompanionGroup("links"));
}

// 2026-10-14: a links APPLY that no IN_PROGRESS proved received, ended "unconfirmed" by the readback, left the earlier
// confirmed change savable and so the unproven candidate with it.
void CompanionControllerTest::_testUnconfirmedLinkApplyBlocksSave()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    primeCompanionWithoutVehicle(controller, 1);
    QVERIFY(runLinkApplyToAccepted(controller, 1, sent, 1, "SIYI", "/dev/ttyAMA0", 57600));
    QVERIFY(controller.companionState()["unsaved"].toBool());

    QVERIFY(startLinkApply(controller, sent)); // no IN_PROGRESS: nothing proves the CC got it
    controller._clearTelemetryState();
    QVERIFY(controller.companionState()["waitingReconnect"].toBool());
    sendCompanionHeartbeat(controller, 1);
    primeParam(controller, 1, "CC_L0_PORT", QString("/dev/ttyUSB0"));
    primeParam(controller, 1, "CC_L0_BAUD", 57600u);
    QTRY_COMPARE_WITH_TIMEOUT(controller.operationState("companion")["outcome"].toString(), QString("unconfirmed"), 3000);
    QVERIFY(!controller.companionState()["unsaved"].toBool());
    QVERIFY(!controller.saveCompanionGroup("links"));
}

// 2026-10-14: a links APPLY the CC never answered (not a refusal) ended "error" and left the earlier confirmed change
// savable, with the candidate the CC may be running.
void CompanionControllerTest::_testSilentLinkApplyBlocksSave()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    primeCompanionWithoutVehicle(controller, 1);
    QVERIFY(runLinkApplyToAccepted(controller, 1, sent, 1, "SIYI", "/dev/ttyAMA0", 57600));
    QVERIFY(controller.companionState()["unsaved"].toBool());

    QVERIFY(startLinkApply(controller, sent));
    QTRY_COMPARE_WITH_TIMEOUT(controller.operationState("companion")["error"].toString(), QString("Companion không phản hồi"), 3000);
    QVERIFY(!controller.companionState()["unsaved"].toBool());
    QVERIFY(!controller.saveCompanionGroup("links"));

    // 2026-10-15: the same values again set nothing (they equal the stage) and sent nothing, so the block never lifted.
    primeParam(controller, 1, "CC_L0_PORT", QString("/dev/ttyUSB0")); // answers the re-read: the stage holds the candidate
    primeParam(controller, 1, "CC_L0_BAUD", 57600u);
    sendSerialLink(controller, 1, 0, 2, "FC", "/dev/ttyUSB0", 57600); // the CC applied it; only its answer was lost
    sent.clear();
    QVERIFY(runLinkApplyToAccepted(controller, 1, sent)); // an APPLY alone, confirmed by telemetry
    QCOMPARE(countSent(sent, MAVLINK_MSG_ID_PARAM_EXT_SET), 0);
    QVERIFY(controller.companionState()["unsaved"].toBool());
    QVERIFY(controller.saveCompanionGroup("links"));
}

// 2026-10-15: an APPLY lost on the wire leaves the candidate staged on the CC; asking for it again found nothing to SET and
// returned without sending the APPLY, so the change could never be applied or saved.
void CompanionControllerTest::_testRetryOfStagedLinkSendsApplyOnly()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    primeCompanionWithoutVehicle(controller, 1);
    primeParam(controller, 1, "CC_L0_PORT", QString("/dev/ttyUSB0")); // staged; the link telemetry still shows the old one
    primeParam(controller, 1, "CC_L0_BAUD", 57600u);
    sent.clear();
    QVERIFY(controller.setLinkConfig(0, "/dev/ttyUSB0", 57600));
    QCOMPARE(sent.size(), 1);
    QCOMPARE(sent.last().msgid, uint32_t(MAVLINK_MSG_ID_COMMAND_LONG));
    mavlink_command_long_t cmd{};
    mavlink_msg_command_long_decode(&sent.last(), &cmd);
    QCOMPARE(cmd.command, static_cast<uint16_t>(MAV_CMD_THACO_APPLY_CONFIG));
    QCOMPARE(cmd.param1, 1.0f);
    QVERIFY(stepTexts(controller.operationState("companion")).contains(QStringLiteral("APPLY links")));
    sendCommandAck(controller, 1, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_ACCEPTED, 0);
    QTRY_COMPARE_WITH_TIMEOUT(([&]() { sendSerialLink(controller, 1, 0, 2, "FC", "/dev/ttyUSB0", 57600);
        return controller.operationState("companion")["outcome"].toString(); })(), QString("confirmed"), 2000);
    QVERIFY(controller.companionState()["unsaved"].toBool());
}

// Nothing staged and nothing unproven: the same values again send nothing (kept by the 2026-10-15 APPLY-only retry).
void CompanionControllerTest::_testUnchangedLinkRequestSendsNothing()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    primeCompanionWithoutVehicle(controller, 1);
    sent.clear();
    QVERIFY(controller.setLinkConfig(0, "/dev/ttyAMA4", 921600));
    QVERIFY(sent.isEmpty());
    QVERIFY(!controller.companionState()["pending"].toBool());
}

// 2026-10-15: a hotspot draft equal to the CC's stage set nothing and sent nothing, so a lost or failed hotspot APPLY could
// not be retried with the same values and SAVE stayed blocked.
void CompanionControllerTest::_testHotspotRetrySendsApplyOnly()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    primeCompanionWithoutVehicle(controller, 1);
    primeHotspot(controller, 1);
    readBackCandidateHotspot(controller); // staged by an APPLY that never ran; the AP still shows "Existing"
    const auto applyOnly = [&]() {
        sent.clear();
        if (!controller.applyHotspot({{"ssid", "Candidate"}, {"ipCidr", "192.168.11.1/24"}, {"dhcpEnabled", true}}) || !commandSent(sent)) return false;
        mavlink_command_long_t cmd{};
        mavlink_msg_command_long_decode(&sent.last(), &cmd);
        return sent.size() == 1 && cmd.command == MAV_CMD_THACO_APPLY_CONFIG && cmd.param1 == 3.0f;
    };
    QVERIFY(applyOnly());
    sendCommandAck(controller, 1, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_FAILED, 0); // the candidate stays active: SAVE blocked
    readBackCandidateHotspot(controller);                          // answers the re-read: the stage resynced to that candidate
    sendNetwork(controller, 1, "Candidate", "192.168.11.1", true); // and the AP shows it, so only the block is left
    QVERIFY(applyOnly());
    sendCommandAck(controller, 1, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_ACCEPTED, 0);
    QCOMPARE(outcomeWhileAp(controller, "Candidate", "192.168.11.1", true), QString("confirmed"));
    QVERIFY(controller.hotspotState()["unsaved"].toBool());
    QVERIFY(controller.saveCompanionGroup("hotspot"));
}

// 2026-10-14: a failed or unconfirmed hotspot APPLY leaves its candidate active on the CC (no rollback); SAVE would
// persist it, so it waits for a confirmed hotspot APPLY.
void CompanionControllerTest::_testFailedHotspotApplyBlocksSave()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    const auto apply = [&](const QString& ssid, uint8_t result) {
        sent.clear();
        if (!controller.applyHotspot({{"ssid", ssid}, {"ipCidr", "192.168.11.1/24"}, {"dhcpEnabled", true}})) return false;
        ackAllSets(controller, 1, sent);
        if (!commandSent(sent)) return false;
        sendCommandAck(controller, 1, MAV_CMD_THACO_APPLY_CONFIG, result, 0);
        if (result != MAV_RESULT_ACCEPTED) return controller.operationState("companion")["outcome"].toString() == QLatin1String("error");
        return QTest::qWaitFor([&]() { sendNetwork(controller, 1, qPrintable(ssid), "192.168.11.1", true);
            return controller.operationState("companion")["outcome"].toString() == QLatin1String("confirmed"); }, 2000);
    };
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    primeCompanionWithoutVehicle(controller, 1);
    primeHotspot(controller, 1);
    QVERIFY(apply("Candidate", MAV_RESULT_ACCEPTED));
    QVERIFY(controller.hotspotState()["unsaved"].toBool());
    QVERIFY(apply("Second", MAV_RESULT_FAILED));
    QVERIFY(!controller.hotspotState()["unsaved"].toBool());
    QVERIFY(!controller.saveCompanionGroup("hotspot"));
    primeParam(controller, 1, "CC_AP_SSID", QString("Second")); // what the CC runs now; answers the re-read
    QVERIFY(apply("Third", MAV_RESULT_ACCEPTED));
    QVERIFY(controller.hotspotState()["unsaved"].toBool());
    QVERIFY(controller.saveCompanionGroup("hotspot"));
}

// 2026-10-14: a link's stage that differed from its telemetry (a lost APPLY, a half-sent stage) refused every links
// change, even writing the old value back, until the CC rebooted. APPLY applies every staged link value, so it goes along.
void CompanionControllerTest::_testPendingStageOfAnotherLinkRidesAlong()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    primeCompanionWithoutVehicle(controller, 1);
    primeParam(controller, 1, "CC_L0_PORT", QString("/dev/ttyUSB0")); // staged on the CC, never applied
    QVERIFY(controller.setLinkConfig(0, "/dev/ttyAMA4", 921600));      // the old value back
    ackParamSet(controller, 1, "CC_L0_PORT", QString("/dev/ttyAMA4"), PARAM_ACK_FAILED); // end it (nothing to roll back)
    primeParam(controller, 1, "CC_L0_PORT", QString("/dev/ttyUSB0")); // answers the re-read: the stage is still there

    sent.clear();
    QVERIFY(controller.setLinkConfig(1, "/dev/ttyAMA0", 57600));
    QVERIFY(stepTexts(controller.operationState("companion")).contains(QStringLiteral("Kèm theo: FC đang chờ áp dụng")));
    QCOMPARE(controller._toolbarOperation->effectCandidate.value("CC_L0_PORT").toString(), QString("/dev/ttyUSB0"));
    ackAllSets(controller, 1, sent);
    QVERIFY(commandSent(sent));
    sendCommandAck(controller, 1, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_ACCEPTED, 0);
    QTRY_COMPARE_WITH_TIMEOUT(([&]() {
        sendSerialLink(controller, 1, 0, 2, "FC", "/dev/ttyUSB0", 921600);
        sendSerialLink(controller, 1, 1, 2, "SIYI", "/dev/ttyAMA0", 57600);
        return controller.operationState("companion")["outcome"].toString(); })(), QString("confirmed"), 2000);
}

// 2026-10-14: the camera popup's mode switch and settings needed a Vehicle (linkUp); Camera/Vision need only the CC (spec §5).
void CompanionControllerTest::_testStreamPopupWithoutVehicle()
{
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
    property alias page: streamPage
    StreamStatusPage { id: streamPage; expanded: true }
}
)QML", QUrl("qrc:/qml/stream-page-no-vehicle-test.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.create());
    QVERIFY2(root, qPrintable(component.errorString()));
    auto controller = qobject_cast<CompanionController*>(root->property("controller").value<QObject*>());
    QVERIFY(controller);
    controller->setSendHookForTest([](const mavlink_message_t&) { return true; });
    primeStreamMode(*controller, 1, 1);
    auto page = root->property("page").value<QObject*>();
    QVERIFY(page);
    QTRY_VERIFY_WITH_TIMEOUT(page->property("editable").toBool(), 2000);
    auto modeSwitch = page->findChild<QObject*>("streamModeSwitch");
    QVERIFY(modeSwitch);
    QVERIFY(modeSwitch->property("enabled").toBool());
}

void CompanionControllerTest::_testApplyWaitReadbackMismatchFails()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    QVERIFY(startHotspotApplyThenLoseCompanion(controller, sent));
    sendCompanionHeartbeat(controller, 1);
    primeParam(controller, 1, "CC_AP_SSID", QString("OldSsid")); // the CC did not apply it
    primeParam(controller, 1, "CC_AP_IP", QString("192.168.11.1/24"));
    primeParam(controller, 1, "CC_AP_DHCP", 1u);
    QTRY_COMPARE_WITH_TIMEOUT(controller.operationState("companion")["outcome"].toString(), QString("error"), 3000);
    QCOMPARE(controller.operationState("companion")["error"].toString(), QString("Không áp dụng được"));
    // 2026-10-14: SAVE is offered only after a confirmed APPLY (it would persist what the CC runs now).
    QVERIFY(!controller.hotspotState()["unsaved"].toBool());
    QVERIFY(!controller.saveCompanionGroup("hotspot"));
}

void CompanionControllerTest::_testCancelCompanionWait()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    QVERIFY(startHotspotApplyThenLoseCompanion(controller, sent));
    controller.cancelCompanionWait();
    QCOMPARE(controller.operationState("companion")["outcome"].toString(), QString("unconfirmed"));
    QVERIFY(!controller.companionState()["pending"].toBool());
}

// 2026-10-14: an operation that is not a links/hotspot APPLY still ends when the CC goes away.
void CompanionControllerTest::_testNonApplyOpEndsOnHeartbeatLoss()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    primeCompanionWithoutVehicle(controller, 1);
    QVERIFY(controller.setLinkConfig(0, "ttyUSB0", 57600)); // first SET sent, no ACK yet
    QTRY_VERIFY_WITH_TIMEOUT(sent.last().msgid == MAVLINK_MSG_ID_PARAM_EXT_SET, 2000);
    controller.setReceivedAgeForTest("heartbeat", 5000);
    QTRY_COMPARE_WITH_TIMEOUT(controller.operationState("companion")["error"].toString(), QString("Mất kết nối Companion"), 2000);
}

// 2026-10-14: links/hotspot are not autosaved any more; the popup says so until the user saves.
void CompanionControllerTest::_testApplyLeavesUnsavedUntilSave()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    primeCompanionWithoutVehicle(controller, 1);
    QVERIFY(!controller.saveCompanionGroup("links")); // nothing applied yet
    QVERIFY(runLinkApplyToAccepted(controller, 1, sent));
    QVERIFY(controller.companionState()["unsaved"].toBool());
    QVERIFY(stepTexts(controller.operationState("companion")).contains(QStringLiteral("Đã áp dụng, chưa lưu — reboot sẽ về cấu hình cũ")));
    // 2026-10-14: a refused SAVE read "Apply bị từ chối"; it is still unsaved and can be tried again.
    QVERIFY(controller.saveCompanionGroup("links"));
    sendCommandAck(controller, 1, MAV_CMD_THACO_SAVE_DEFAULT_CONFIG, MAV_RESULT_FAILED, 0);
    QCOMPARE(controller.operationState("companion")["error"].toString(), QString("Lưu bị từ chối (4)"));
    QVERIFY(controller.companionState()["unsaved"].toBool());
    QVERIFY(controller.saveCompanionGroup("links"));
    mavlink_command_long_t cmd{};
    mavlink_msg_command_long_decode(&sent.last(), &cmd);
    QCOMPARE(cmd.command, static_cast<uint16_t>(MAV_CMD_THACO_SAVE_DEFAULT_CONFIG));
    QCOMPARE(cmd.param1, 1.0f);
    QCOMPARE(cmd.target_system, uint8_t(1));
    QCOMPARE(cmd.target_component, uint8_t(191));
    sendCommandAck(controller, 1, MAV_CMD_THACO_SAVE_DEFAULT_CONFIG, MAV_RESULT_ACCEPTED, 0);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.companionState()["unsaved"].toBool(), 2000);
    QCOMPARE(controller.operationState("companion")["outcome"].toString(), QString("confirmed"));
}

// 2026-10-14: the number of links comes from the CC, not from "two links" in QGC.
void CompanionControllerTest::_testThirdLinkIsConfigurable()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    primeCompanionWithoutVehicle(controller, 1);
    sendSerialLink(controller, 1, /*index*/ 2, /*count*/ 3, "GPS", "/dev/ttyUSB1", 57600);
    primeParam(controller, 1, "CC_L2_PORT", QString("/dev/ttyUSB1"));
    primeParam(controller, 1, "CC_L2_BAUD", 57600u);
    QVERIFY(!controller.setLinkConfig(2, "/dev/ttyAMA0", 115200)); // link 1's port, now checked against every link
    QVERIFY(controller.setLinkConfig(2, "/dev/ttyUSB1", 115200));
    QTRY_VERIFY_WITH_TIMEOUT(sent.last().msgid == MAVLINK_MSG_ID_PARAM_EXT_SET, 2000);
    mavlink_param_ext_set_t set{};
    mavlink_msg_param_ext_set_decode(&sent.last(), &set);
    QCOMPARE(QString::fromLatin1(set.param_id, qstrnlen(set.param_id, 16)), QString("CC_L2_BAUD"));
}

// 2026-10-14: Camera and Vision need only the CC heartbeat; reason 1 "Chưa kết nối vehicle" blocked the mode switch.
void CompanionControllerTest::_testStreamModeWithoutVehicle()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    QCOMPARE(controller.streamReason("camera")["code"].toInt(), 2); // no CC heartbeat yet
    primeStreamMode(controller, 1, 1);
    QCOMPARE(controller.streamReason("camera")["code"].toInt(), 0);
    QVERIFY(controller.setStreamMode(2));
    QCOMPARE(sent.last().msgid, uint32_t(MAVLINK_MSG_ID_PARAM_EXT_SET));
}

// 2026-10-14: the rollback of a half-sent stage needed a disarmed Vehicle; with the FC silent the first SET stayed staged.
void CompanionControllerTest::_testStageRollbackWithoutVehicle()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    primeCompanionWithoutVehicle(controller, 1);
    QVERIFY(controller.setLinkConfig(0, "/dev/ttyUSB0", 57600));
    ackParamSet(controller, 1, "CC_L0_PORT", QString("/dev/ttyUSB0"));
    ackParamSet(controller, 1, "CC_L0_BAUD", 57600u, PARAM_ACK_FAILED);
    QCOMPARE(sent.last().msgid, uint32_t(MAVLINK_MSG_ID_PARAM_EXT_SET));
    mavlink_param_ext_set_t set{};
    mavlink_msg_param_ext_set_decode(&sent.last(), &set);
    QCOMPARE(QString::fromLatin1(set.param_id, qstrnlen(set.param_id, 16)), QString("CC_L0_PORT"));
    QCOMPARE(QString::fromLatin1(set.param_value), QString("/dev/ttyAMA4"));
}

// 2026-10-14: the CC never echoes the passphrase; the readback after a reconnect must not wait for it (or show it).
void CompanionControllerTest::_testReadbackSkipsSecret()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    QVERIFY(startHotspotApplyThenLoseCompanion(controller, sent, 1, QStringLiteral("Secret-Canary-123")));
    sendCompanionHeartbeat(controller, 1);
    readBackCandidateHotspot(controller);
    // 2026-10-15: the hotspot is confirmed by the AP's telemetry after the reconnect, no longer by this readback.
    QCOMPARE(outcomeWhileAp(controller, "Candidate", "192.168.11.1", true), QString("confirmed"));
    for (const auto& text : stepTexts(controller.operationState("companion"))) QVERIFY2(!text.contains("Secret-Canary-123"), qPrintable(text));
}

// 2026-10-14: the telemetry watchdog (3.5 s without CC telemetry) ended a hotspot APPLY that waited for the AP to return.
void CompanionControllerTest::_testTelemetryTimeoutKeepsApplyWait()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    QVERIFY(startHotspotApply(controller, sent)); // IN_PROGRESS 35, not waiting yet: the watchdog fires first
    controller._clearTelemetryState(); // what the watchdog does
    QVERIFY(controller.operationState("companion")["active"].toBool());
    QVERIFY(controller.companionState()["waitingReconnect"].toBool());
    sendCompanionHeartbeat(controller, 1);
    readBackCandidateHotspot(controller);
    // 2026-10-15: the hotspot is confirmed by the AP's telemetry after the reconnect, no longer by this readback.
    QCOMPARE(outcomeWhileAp(controller, "Candidate", "192.168.11.1", true), QString("confirmed"));
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
    camera.config_version = 1;
    camera.fpv_enabled = 1;
    camera.supported_codecs = 1;
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
)QML",
                              QUrl("qrc:/qml/toolbar-readback-test.qml"));
        QVERIFY2(readComponent.isReady(), qPrintable(readComponent.errorString()));
        std::unique_ptr<QObject> readPage(readComponent.create());
        QVERIFY2(readPage, qPrintable(readComponent.errorString()));
        auto uiController = qobject_cast<CompanionController *>(readPage->property("controller").value<QObject *>());
        QVERIFY(uiController);
        // 2026-10-14: the CC heartbeat, not the Vehicle, starts the CC session and its first read, on the heartbeat's link.
        const int readsAtStart = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_REQUEST_LIST);
        sendCompanionHeartbeat(*uiController, system, mockLink());
        QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_REQUEST_LIST), readsAtStart + 1, 1500);
        // 2026-10-19: end the session-start LIST first; a readback requested while it runs is now a no-op (no second LIST).
        primeParam(*uiController, system, "CC_L0_BAUD", 921600u);
        QVERIFY(!uiController->ccParametersLoading());
        const int readsBefore = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_REQUEST_LIST);
        const int writesBefore = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
        uiController->processMessageForTest(message);
        QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_REQUEST_LIST),
                                  readsBefore + 1, 1500);
        uiController->processMessageForTest(message);
        QCOMPARE(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_REQUEST_LIST), readsBefore + 1);
        QCOMPARE(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), writesBefore);
    }
    mavlink_message_t sent{};
    mavlink_param_ext_set_t parameter{};
    const auto stage = [&](const char* id, const QVariant& input, uint8_t type, bool ack = false,
                           uint8_t result = PARAM_ACK_ACCEPTED) {
        char raw[128]{};
        if (type == MAV_PARAM_EXT_TYPE_CUSTOM) {
            const auto bytes = input.toString().toUtf8();
            std::memcpy(raw, bytes.constData(), qMin(bytes.size(), qsizetype(127)));
        } else if (type == MAV_PARAM_EXT_TYPE_UINT32) {
            const uint32_t number = input.toUInt();
            std::memcpy(raw, &number, sizeof(number));
        } else
            raw[0] = input.toUInt();
        mavlink_message_t packet{};
        if (ack) mavlink_msg_param_ext_ack_pack(system, 191, &packet, id, raw, type, result);
        else mavlink_msg_param_ext_value_pack(system, 191, &packet, id, raw, type, 1, 0); // 2026-10-14: count 1 ends each read
        controller.processMessageForTest(packet);
    };
    const auto links = [&](uint32_t baud) {
        mavlink_message_t first{}, second{};
        packLinksForTest(system, 191, &first, baud, 115200, 0, 0, 0, 3, "ttyUSB0", "ttyUSB1", &second);
        controller.processMessageForTest(first);
        controller.processMessageForTest(second);
        mavlink_cc_serial_link_t extra{};
        extra.link_index = 2;
        extra.link_count = 3;
        extra.baudrate = 38400;
        extra.status = 2;
        copyField(extra.name, "Additional device");
        copyField(extra.port, "ttyUSB2");
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
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavCommandCount(MAV_CMD_THACO_APPLY_CONFIG, 191),
                              baudCommandsBefore + 1, 3000);
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
    network.config_version = 1;
    network.ap_status = 1;
    network.ap_prefix_length = 24;
    network.ap_dhcp_enabled = 1;
    copyField(network.ap_ssid, "Existing");
    copyField(network.ap_ip, "192.168.10.1");
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
    QTRY_VERIFY_WITH_TIMEOUT(
        [&]() {
        mockLink()->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_PARAM_EXT_SET, sent);
        mavlink_msg_param_ext_set_decode(&sent, &parameter);
            return QString::fromLatin1(parameter.param_id) == "CC_AP_SSID" &&
                   QString::fromLatin1(parameter.param_value) == "Existing";
        }(),
        3000);
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
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET),
                              timeoutSetsBefore + 2, 6500);
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
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), beforeCancel + 1,
                              3000);
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
    cam.config_version = 1;
    cam.supported_codecs = 1;
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
    QVERIFY(
        std::any_of(texts.cbegin(), texts.cend(), [](const QString& t) { return t.contains("Router mở lại cổng"); }));
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

    QTRY_COMPARE_WITH_TIMEOUT(([&]() {
                                  sendLinks(controller, system, 921600, 57600);
                                  return controller.operationState("companion")["outcome"].toString();
                              })(),
                              QString("confirmed"), 4000);
    for (const auto& step : controller.operationState("companion")["steps"].toList())
        QVERIFY(step.toMap()["state"].toString() != "error");
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
    QTRY_VERIFY_WITH_TIMEOUT(
        stepTexts(controller.operationState("companion"))
            .contains(QStringLiteral(
                "Đã gửi cấu hình hotspot mới. Mất kết nối WiFi là bình thường; hãy nối lại vào SSID mới")),
        3000);
    QVERIFY(controller.operationState("companion")["error"].toString().isEmpty());

    QTRY_COMPARE_WITH_TIMEOUT(([&]() {
                                  sendNetwork(controller, system, "Candidate", "192.168.11.1", true);
                                  return controller.operationState("companion")["outcome"].toString();
                              })(),
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
    QVERIFY(stepTexts(controller.operationState("companion"))
                .contains(QStringLiteral("(rollback) PARAM_EXT_SET CC_L1_BAUD=115200")));
}

// 2026-10-12: operation steps are visible to every GCS user; a passphrase must never be in them.
void CompanionVehicleLifecycleTest::_testSecretNeverAppearsInOperationSteps()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    primeLinksAndParams(controller, system, mockLink());
    primeHotspot(controller, system);
    QVERIFY(controller.applyHotspot({{"ssid", "Existing"},
                                     {"ipCidr", "192.168.10.1/24"},
                                     {"dhcpEnabled", true},
                                     {"password", "Secret-Canary-123"}}));
    QTRY_COMPARE_WITH_TIMEOUT(lastSetId(mockLink()), QString("CC_AP_PASS"), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(controller.operationState("companion")["steps"].toList().size() >= 1, true, 1000);
    ackParamSet(controller, system, "CC_AP_PASS", QString()); // a secret comes back empty
    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavCommandCount(MAV_CMD_THACO_APPLY_CONFIG, 191) >= 1, 3000);
    const auto texts = stepTexts(controller.operationState("companion"));
    QVERIFY(texts.contains(QStringLiteral("PARAM_EXT_SET CC_AP_PASS=<hidden>")));
    for (const auto& t : texts)
        QVERIFY2(!t.contains("Secret-Canary-123"), qPrintable(t));
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
    for (const char* bad : {"", "rtmp://host/live", "rtsp://host", "not a url"})
        QVERIFY2(!controller.setStreamParameter("camera", "CC_CAM_PUB_URL", bad), bad);
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
    for (const char* bad :
         {"rtsp://drone.local:8554/camera/", "rtsp://drone.local:8554//camera", "rtsp://h/a/../camera",
          "rtsp://h/./camera", "rtsp://h/%63amera", "rtsp://h/my cam", "rtsp://h/ca$mera"}) {
        QVERIFY2(!controller.setStreamParameter("camera", "CC_CAM_PUB_URL", bad), bad);
        if (!QByteArray(bad).contains(' '))
            QVERIFY2(
                stepTexts(controller.operationState("stream"))
                    .contains(QStringLiteral(
                        "Đường dẫn RTSP chỉ dùng A-Z a-z 0-9 _ . ~ / - (không có đoạn rỗng, . hoặc .., không có %)")),
                bad);
    }
    QCOMPARE(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), base);
    for (const char* good :
         {"rtsp://drone.local:8554/camera", "rtsp://h:8554/live/cam-1_a.b~c", "rtsp://h/camera?x=%41"}) {
        QVERIFY2(controller.setStreamParameter("camera", "CC_CAM_PUB_URL", good), good);
        QTRY_COMPARE_WITH_TIMEOUT(lastSetId(mockLink()), QString("CC_CAM_PUB_URL"), 3000);
        ackParamSet(controller, system, "CC_CAM_PUB_URL", QString(good));
        QTRY_COMPARE_WITH_TIMEOUT(controller.operationState("stream")["outcome"].toString(), QString("confirmed"),
                                  3000);
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
    QCOMPARE(controller.operationState("stream")["error"].toString(),
             QString("Camera: path /other is held by another publisher"));
}

// 2026-10-12: popups said only "Controls require current Companion firmware and telemetry".
void CompanionVehicleLifecycleTest::_testStreamReasonTable()
{
    CompanionController controller;
    QCOMPARE(controller.streamReason("vision")["code"].toInt(), 2); // 2026-10-14: no reason 1 ("no vehicle") any more (spec §5)
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
    // 2026-10-19: was 7 and pinned the lock; a missing CC_VIS_RES is now only "reading".
    QCOMPARE(controller.streamReason("vision")["code"].toInt(), 0);
    QVERIFY(controller.visionStreamState()["reading"].toBool());
    controller.setReceivedAgeForTest("vision", 5000);
    QCOMPARE(controller.streamReason("vision")["code"].toInt(), 4);
    QVERIFY(controller.streamReason("vision")["text"].toString().contains("lần cuối"));
    sendStreamTelemetry(controller, system, false, true, 0, 1);
    primeParam(controller, system, "CC_VIS_RES", QString("640x480"));
    QCOMPARE(controller.streamReason("vision")["code"].toInt(), 0);
    QCOMPARE(controller.visionStreamState()["reason"].toMap()["code"].toInt(), 0);
    // 2026-10-16: every popup open and the 30 s resync reloaded the list and greyed out values that were already known
    controller.requestCcParameters();
    QCOMPARE(controller.streamReason("vision")["code"].toInt(), 0);

    primeLinksAndParams(controller, system, mockLink());
    QVERIFY(controller.setLinkConfig(1, "ttyUSB1", 57600)); // an operation started from the Companion popup
    QCOMPARE(controller.streamReason("vision")["code"].toInt(), 9);
    controller.requestCcParameters();
    // Replaces the old expectation 6 (2026-10-16): it pinned the grey-out while CC_VIS_RES was already confirmed.
    QCOMPARE(controller.streamReason("vision")["code"].toInt(), 9);
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
    QTRY_VERIFY_WITH_TIMEOUT(([&]() {
                                 sendLinks(controller, system, 921600, 57600);
                                 return controller.operationState("companion")["active"].toBool();
                             })(),
                             2000);  // baud alone is not enough
    mavlink_message_t first{}, second{};
    QTRY_COMPARE_WITH_TIMEOUT(([&]() {
                                  packLinksForTest(system, 191, &first, 921600, 57600, 0, 0, 0, 3, "ttyUSB0", "ttyUSB2",
                                                   &second);
                                  controller.processMessageForTest(first);
                                  controller.processMessageForTest(second);
                                  return controller.operationState("companion")["outcome"].toString();
                              })(),
                              QString("confirmed"), 4000);
}

// 2026-10-12: with the AP off the hotspot popup was always blocked, leaving SSH as the only recovery.
// 2026-10-14: renamed from _testHotspotApplyAllowedWhenApOff. An AP up but broadcasting something else than the stage was
// refused ("existing stage; reconcile", asserted here before) until the CC rebooted; there is no reconcile step any more
// (spec §3) and the draft re-SETs SSID/IP/DHCP, so it is allowed like an AP that is off.
void CompanionVehicleLifecycleTest::_testHotspotApplyAllowedWhenStageDiffers()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const auto system = vehicle()->id();
    primeLinksAndParams(controller, system, mockLink());
    primeHotspot(controller, system);
    sendNetwork(controller, system, "Other", "192.168.10.1", true, true); // AP up, but not what the stage says
    const int base = mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET);
    QVERIFY(controller.applyHotspot({{"ssid", "Candidate"}, {"ipCidr", "192.168.11.1/24"}, {"dhcpEnabled", true}}));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PARAM_EXT_SET), base + 1, 3000);
    QCOMPARE(lastSetId(mockLink()), QString("CC_AP_SSID"));
}

// 2026-10-12: Vision confidence had no control once Settings was removed. (2026-10-13: the AI branch switch is gone;
// the mode drives it.)
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
    QTRY_COMPARE_WITH_TIMEOUT(controller.streamModeState()["operation"].toMap()["outcome"].toString(),
                              QString("confirmed"), 2000);
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
        if (++beat % 2)
            ackParamSet(controller, system, "CC_STREAM_MODE", 1u, PARAM_ACK_IN_PROGRESS);
        else
            sendCompanionText(controller, system, "Camera: waiting for first frame");
        stayedActive = stayedActive && controller.streamModeState()["operation"].toMap()["active"].toBool();
    });
    keepAlive.start(100);
    QElapsedTimer clock;
    clock.start();
    QTRY_VERIFY_WITH_TIMEOUT(clock.elapsed() > 2000, 4000); // longer than every old ACK/total budget of the unit tests
    keepAlive.stop();
    QVERIFY(stayedActive);
    QTRY_COMPARE_WITH_TIMEOUT(controller.streamModeState()["operation"].toMap()["outcome"].toString(),
                              QString("unconfirmed"), 2000);
    const auto operation = controller.streamModeState()["operation"].toMap();
    QVERIFY(stepTexts(operation).contains(QStringLiteral("CC không phản hồi")));
    QVERIFY(operation["error"].toString().isEmpty());
    QCOMPARE(controller.streamModeState()["pendingMode"].toInt(), controller.streamModeState()["mode"].toInt());
}

// 2026-10-13: Cancel sends the previous mode again; the CC fails the cancelled operation first, which must not end the
// new one.
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
    QTRY_COMPARE_WITH_TIMEOUT(controller.streamModeState()["operation"].toMap()["outcome"].toString(),
                              QString("confirmed"), 2000);
    QCOMPARE(controller.streamModeState()["mode"].toInt(), 1);
    QVERIFY(stepTexts(controller.streamModeState()["operation"].toMap())
                .contains(QStringLiteral("Camera: [3/5] waiting: MediaMTX holds /camera")));
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

// 2026-10-13: the CC may send one more keepalive just after the FAILED of the same parameter; it must not revive
// anything.
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
)QML",
                      QUrl("qrc:/qml/stream-mode-switch-test.qml"));
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
    QVERIFY(
        controller.streamModeState()["blocked"].toString().isEmpty());  // the switch stays usable: it is the way out

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
    QTRY_COMPARE_WITH_TIMEOUT(controller.streamModeState()["operation"].toMap()["outcome"].toString(),
                              QString("confirmed"), 2000);
    QCOMPARE(controller.streamModeState()["mode"].toInt(), 2);
}

// 2026-10-13: a mode SET does not cancel a Vision inference op (CC_VIS_CONF/FPS run in their own CC slot), and QGC
// tracks one op: the switch is refused with a visible reason instead of doing nothing.
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

// 2026-10-13: the player restarted only when QGC's own mode op completed, so a late ACCEPTED or a readback that moved
// the mode left it stopped while the CC streamed (black video in flight).
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

// 2026-10-13: a keepalive of the replaced mode op can arrive after the new SET; it cleared expectCancelAck, so the
// replaced op's FAILED then ended the new op with an error.
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
    ackParamSet(controller, system, "CC_STREAM_MODE", 1u,
                PARAM_ACK_IN_PROGRESS);  // keepalive of 1 -> 2, sent before the CC got the SET
    ackParamSet(controller, system, "CC_STREAM_MODE", 1u, PARAM_ACK_FAILED);      // the CC cancels 1 -> 2
    QVERIFY(controller.streamModeState()["operation"].toMap()["active"].toBool());
    ackParamSet(controller, system, "CC_STREAM_MODE", 1u, PARAM_ACK_IN_PROGRESS);
    ackParamSet(controller, system, "CC_STREAM_MODE", 0u);
    QTRY_COMPARE_WITH_TIMEOUT(controller.streamModeState()["operation"].toMap()["outcome"].toString(),
                              QString("confirmed"), 2000);
    QCOMPARE(controller.streamModeState()["mode"].toInt(), 0);
}

// 2026-10-13: replacing a mode op while the new one could not start (the replacing SET cannot be sent) dropped the old op and
// published nothing; the old op must stay and the refusal must be visible.
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
    // 2026-10-19: the replacing SET used to be refused because a LIST was loading; now it is refused because it cannot be sent.
    int refusedSets = 0;
    controller.setSendHookForTest([&refusedSets](const mavlink_message_t& m) {
        if (m.msgid == MAVLINK_MSG_ID_PARAM_EXT_SET) {
            ++refusedSets;
        }
        return false;
    });
    const auto stepsBefore = controller.operationState("stream")["steps"].toList().size();

    QVERIFY(!controller.setStreamMode(0));
    QCOMPARE(refusedSets, 1); // the replacing SET was tried once and not retried
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
    // 2026-10-15: the op this change ended must not leave a SAVE block behind; the switch clears the flags (findings r2).
    QVERIFY(controller._saveBlocked.isEmpty());

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

// 2026-10-14: a links APPLY that changes the radio's baud drops the FC's Vehicle too; the wait must outlive it, but a CC of
// another system that answers first must not settle it.
// 2026-10-14: that end was the error "Mất kết nối Companion" (asserted here before); the first CC may well have applied the
// change, so it is "unconfirmed" now.
void CompanionVehicleLifecycleTest::_testApplyWaitAcrossVehicleLoss()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    const uint8_t system = vehicle()->id();
    QList<mavlink_message_t> sent;
    QVERIFY(startHotspotApplyThenLoseCompanion(controller, sent, system));
    _disconnectMockLink();
    QVERIFY(!controller.vehicleAvailable());
    QVERIFY(controller.companionState()["waitingReconnect"].toBool());
    sendCompanionHeartbeat(controller, system + 1); // no Vehicle: the first CC heard locks
    QCOMPARE(controller.operationState("companion")["outcome"].toString(), QString("unconfirmed"));
    QVERIFY(stepTexts(controller.operationState("companion")).contains(QStringLiteral("CC khác đã kết nối lại")));
    QVERIFY(!controller.hotspotState()["unsaved"].toBool());
}

// 2026-10-14: "applied, not saved" belongs to the CC that confirmed the APPLY, not to whichever CC QGC locks onto next.
void CompanionVehicleLifecycleTest::_testUnsavedClearedWhenCompanionLockResets()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    const uint8_t other = 42;
    primeCompanionWithoutVehicle(controller, other); // no Vehicle: the first CC heard locks
    QVERIFY(runLinkApplyToAccepted(controller, other, sent));
    QVERIFY(controller.companionState()["unsaved"].toBool());
    _connectMockLinkNoInitialConnectSequence();
    QVERIFY(vehicle()->id() != other);
    QVERIFY(!controller.companionState()["unsaved"].toBool());
}

// 2026-10-14: the lock reset when the Vehicle went away with the CC stale cleared "applied, not saved" although the same CC
// came back still running the unsaved change.
void CompanionVehicleLifecycleTest::_testUnsavedKeptWhenSameCompanionReturns()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    _connectMockLinkNoInitialConnectSequence();
    const uint8_t system = vehicle()->id();
    primeCompanionWithoutVehicle(controller, system);
    QVERIFY(runLinkApplyToAccepted(controller, system, sent));
    controller.setReceivedAgeForTest("heartbeat", 5000);
    _disconnectMockLink(); // the Vehicle goes while the CC is stale: the lock resets
    QCOMPARE(controller.sourceSystemId(), -1);
    sendCompanionHeartbeat(controller, system);
    QCOMPARE(controller.sourceSystemId(), int(system));
    QVERIFY(controller.companionState()["unsaved"].toBool());
}

// 2026-10-15: a links APPLY cancelled while the Vehicle loss had reset the CC lock left Lưu offered when the same CC locked
// again; SAVE then persisted the cancelled, unproven candidate.
void CompanionVehicleLifecycleTest::_testCancelDuringLockResetBlocksSave()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    _connectMockLinkNoInitialConnectSequence();
    const uint8_t system = vehicle()->id();
    primeCompanionWithoutVehicle(controller, system);
    QVERIFY(runLinkApplyToAccepted(controller, system, sent, 1, "SIYI", "/dev/ttyAMA0", 57600));
    QVERIFY(controller.companionState()["unsaved"].toBool());
    QVERIFY(startLinkApply(controller, sent, system));
    sendCommandAck(controller, system, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_IN_PROGRESS, 50);
    controller.setReceivedAgeForTest("heartbeat", 5000);
    QTRY_VERIFY_WITH_TIMEOUT(controller.companionState()["waitingReconnect"].toBool(), 2000);
    _disconnectMockLink(); // the Vehicle goes while the CC is stale: the lock resets
    QCOMPARE(controller.sourceSystemId(), -1);
    controller.cancelCompanionWait();
    QCOMPARE(controller.operationState("companion")["outcome"].toString(), QString("unconfirmed"));
    sendCompanionHeartbeat(controller, system); // the same CC locks again
    QCOMPARE(controller.sourceSystemId(), int(system));
    QVERIFY(!controller.companionState()["unsaved"].toBool());
    QVERIFY(!controller.saveCompanionGroup("links"));

    sendSerialLink(controller, system, 0, 2, "FC", "/dev/ttyAMA4", 921600); // its telemetry and parameters after the loss
    sendSerialLink(controller, system, 1, 2, "SIYI", "/dev/ttyAMA0", 57600);
    primeParam(controller, system, "CC_L0_PORT", QString("/dev/ttyUSB0")); // the stage still holds the cancelled candidate
    primeParam(controller, system, "CC_L0_BAUD", 57600u);
    primeParam(controller, system, "CC_L1_PORT", QString("/dev/ttyAMA0"));
    primeParam(controller, system, "CC_L1_BAUD", 57600u);
    sent.clear();
    QVERIFY(runLinkApplyToAccepted(controller, system, sent)); // the same candidate again, confirmed
    QVERIFY(controller.companionState()["unsaved"].toBool());
    QVERIFY(controller.saveCompanionGroup("links"));
}

// 2026-10-14: the port list is the CC's boot scan (CC_SER_PORTS), not the ports links already use.
void CompanionControllerTest::_testAvailablePortsFromCompanion()
{
    CompanionController controller;
    controller.setSendHookForTest([](const mavlink_message_t&) { return true; });
    QSignalSpy portsSpy(&controller, &CompanionController::availablePortsChanged);
    sendCompanionHeartbeat(controller, 1);
    sendSerialLink(controller, 1, 0, 2, "FC", "/dev/ttyAMA4", 921600);
    sendSerialLink(controller, 1, 1, 2, "SIYI", "/dev/ttyAMA0", 115200);
    QVERIFY(controller.availablePorts().isEmpty()); // the CC has not reported its list yet
    portsSpy.clear();
    primeParam(controller, 1, "CC_SER_PORTS", QString("/dev/ttyAMA4,/dev/ttyAMA0,/dev/ttyUSB0"));
    QCOMPARE(controller.availablePorts(), (QStringList{"/dev/ttyAMA4", "/dev/ttyAMA0", "/dev/ttyUSB0"}));
    QVERIFY(portsSpy.count() >= 1);
}

// 2026-10-14: the popup's baud list was an array in the QML; it is the catalog's options of that link's CC_L<n>_BAUD
// (milestone B: the CC's metadata), and a link the CC has not described has none.
void CompanionControllerTest::_testBaudOptionsFromCatalog()
{
    CompanionController controller;
    controller.setSendHookForTest([](const mavlink_message_t&) { return true; });
    QVERIFY(controller.baudOptions(0).contains("57600"));
    QCOMPARE(controller.baudOptions(1), controller._paramService->options("CC_L1_BAUD"));
    QVERIFY(controller.baudOptions(2).isEmpty());
    sendCompanionHeartbeat(controller, 1);
    primeParam(controller, 1, "CC_L2_BAUD", 38400u);
    QCOMPARE(controller.baudOptions(2), controller.baudOptions(0));
}

// 2026-10-15: the popup's Update button was disabled for the current values, so a SAVE block or a lost APPLY could not be
// retried from it; needsApply says when the same values would still send an APPLY (the beginToolbarOperation rule).
void CompanionControllerTest::_testNeedsApplyMatchesApplyOnlyRule()
{
    CompanionController controller;
    controller.setSendHookForTest([](const mavlink_message_t&) { return true; });
    primeCompanionWithoutVehicle(controller, 1);
    primeHotspot(controller, 1);
    QVERIFY(!controller.companionState()["needsApply"].toBool());
    QVERIFY(!controller.hotspotState()["needsApply"].toBool());

    controller._saveBlocked["links"] = true; // the last links APPLY was not confirmed
    QVERIFY(controller.companionState()["needsApply"].toBool());
    QVERIFY(!controller.hotspotState()["needsApply"].toBool());
    controller._saveBlocked.remove("links");
    primeParam(controller, 1, "CC_L0_PORT", QString("/dev/ttyUSB0")); // staged; the link telemetry still shows the old port
    QVERIFY(controller.companionState()["needsApply"].toBool());

    controller._saveBlocked["hotspot"] = true;
    QVERIFY(controller.hotspotState()["needsApply"].toBool());
    controller._saveBlocked.remove("hotspot");
    QVERIFY(!controller.hotspotState()["needsApply"].toBool());
    readBackCandidateHotspot(controller); // staged; the AP still shows "Existing"
    QVERIFY(controller.hotspotState()["needsApply"].toBool());
}

// 2026-10-14: without the parameter catalog QGC fell back to a built-in table of links; now nothing is configurable and
// both popups say why.
void CompanionControllerTest::_testCatalogLoadErrorBlocksCompanion()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    controller._paramService->loadTemplate(QStringLiteral(":/missing/cc_parameters.json"));
    primeCompanionWithoutVehicle(controller, 1);
    sendStreamTelemetry(controller, 1, true, true, 2);
    primeParam(controller, 1, "CC_CAM_RES", QString("1280x720"));
    const QString text = QStringLiteral("Chưa tải được danh sách tham số Companion");
    QCOMPARE(controller.companionState()["loadError"].toString(), text);
    QCOMPARE(controller.streamReason("camera")["code"].toInt(), 7);
    QCOMPARE(controller.streamReason("camera")["text"].toString(), text);
    sent.clear();
    QVERIFY(!controller.setLinkConfig(0, "/dev/ttyUSB0", 57600));
    QVERIFY(!controller.setStreamParameter("camera", "CC_CAM_FPS", 15));
    QCOMPARE(countSent(sent, MAVLINK_MSG_ID_PARAM_EXT_SET), 0);
}

// 2026-10-19: codes 6/7 locked every control until the list had read CC_CAM_RES, 2.5-3 s after connect.
void CompanionControllerTest::_testStreamPopupIsNotLockedByMissingValues()
{
    CompanionController controller;
    controller.setSendHookForTest([&](const mavlink_message_t&) { return true; });
    sendCompanionHeartbeat(controller, 1); // a LIST runs, nothing read yet
    sendStreamTelemetry(controller, 1, true, true, 2);
    QVERIFY(controller.ccParametersLoading());
    QCOMPARE(controller.streamReason("camera")["code"].toInt(), 0);
    QVERIFY(controller.cameraStreamState()["reading"].toBool());
    QVERIFY(controller.modeBlockedReason().isEmpty());
    primeParam(controller, 1, "CC_CAM_RES", QString("1280x720")); // this helper ends the LIST
    QVERIFY(!controller.cameraStreamState()["reading"].toBool());
    // 2026-10-19: a resync LIST while the values are known must not show "Đang đọc tham số…" (it flashed every 30 s)
    controller.requestCcParameters();
    QVERIFY(controller.ccParametersLoading());
    QVERIFY(!controller.cameraStreamState()["reading"].toBool());
}

namespace {
// The Companion popup bound to the engine's CompanionController singleton, which is returned in `controller`.
std::unique_ptr<QObject> loadCompanionPage(QQmlEngine& engine, CompanionController*& controller)
{
    engine.addImageProvider(QLatin1String(ColoredSvgImageProvider::ProviderId), new ColoredSvgImageProvider());
    engine.addImportPath("qrc:/qml");
    QQmlComponent component(&engine);
    component.setData(R"QML(
import QtQuick
import QGroundControl
import Custom.AgriDrone
Item {
    property var controller: CompanionController
    property alias page: companionPage
    CompanionStatusPage { id: companionPage; expanded: true }
}
)QML", QUrl("qrc:/qml/companion-page-test.qml"));
    if (!component.isReady()) qWarning() << component.errorString();
    std::unique_ptr<QObject> root(component.create());
    controller = root ? qobject_cast<CompanionController*>(root->property("controller").value<QObject*>()) : nullptr;
    return root;
}

// findChild() misses Repeater delegates (no QObject parent), so the visual item tree is walked too.
QObject* findItem(QObject* parent, const QString& name)
{
    if (!parent || parent->objectName() == name) return parent;
    QObjectList children = parent->children();
    if (auto* item = qobject_cast<QQuickItem*>(parent)) {
        for (QQuickItem* child : item->childItems()) children.append(child);
    }
    for (QObject* child : std::as_const(children)) {
        if (QObject* found = findItem(child, name)) return found;
    }
    return nullptr;
}

QStringList comboTexts(QObject* combo)
{
    QStringList texts;
    for (int i = 0; combo && i < combo->property("count").toInt(); ++i) {
        QString text;
        QMetaObject::invokeMethod(combo, "textAt", Q_RETURN_ARG(QString, text), Q_ARG(int, i));
        texts << text;
    }
    return texts;
}
}  // namespace

namespace {
void sendBandwidth(CompanionController& c, uint8_t system, uint8_t valid, float videoIn = 2480.f)
{
    mavlink_cc_bandwidth_t bw{};
    bw.valid = valid;
    bw.video_in_kbps = videoIn;
    bw.video_out_kbps[0] = videoIn;
    bw.video_readers = 1;
    mavlink_message_t m{};
    mavlink_msg_cc_bandwidth_encode(system, 191, &m, &bw);
    c.processMessageForTest(m);
}
}  // namespace

// 2026-10-20: per-hop bandwidth (spec 2026-10-20-stream-bandwidth-design.md): QGC could not tell WiFi, RF or a container apart.
void CompanionControllerTest::_testBandwidthFromCompanion()
{
    CompanionController c;
    c.setSendHookForTest([](const mavlink_message_t&) { return true; }); // the heartbeat starts a PARAM_EXT read
    sendCompanionHeartbeat(c, 1);
    sendBandwidth(c, 1, CC_BANDWIDTH_VALID_ROUTER | CC_BANDWIDTH_VALID_CAMERA | CC_BANDWIDTH_VALID_NETWORK);
    const QVariantMap bw = c.bandwidth();
    QCOMPARE(bw["ccState"].toString(), QStringLiteral("ok"));
    QVERIFY(bw["cameraValid"].toBool());
    QCOMPARE(bw["video"].toMap()["inKbps"].toDouble(), 2480.0);
}

void CompanionControllerTest::_testBandwidthPanel()
{
    QQmlEngine engine;
    CompanionController* controller = nullptr;
    auto root = loadCompanionPage(engine, controller);
    QVERIFY(root && controller);
    controller->setSendHookForTest([](const mavlink_message_t&) { return true; }); // the heartbeat starts a PARAM_EXT read
    sendCompanionHeartbeat(*controller, 1);
    sendBandwidth(*controller, 1, CC_BANDWIDTH_VALID_ROUTER | CC_BANDWIDTH_VALID_NETWORK); // edge-camera silent
    emit controller->bandwidthChanged();
    QTRY_VERIFY(findItem(root.get(), QStringLiteral("bandwidthCameraRow")));
    QObject* row = findItem(root.get(), QStringLiteral("bandwidthCameraRow"));
    QTRY_VERIFY(row->property("b").toString().contains(QStringLiteral("edge-camera")));

    controller->_bandwidth.resetCc(); // a CC that never sends CC_BANDWIDTH
    // A real 3 s session: the clock of a fresh controller is younger than 4 s, so a back-dated start would be negative (= no session).
    QTest::qWait(1600);
    sendCompanionHeartbeat(*controller, 1); // keeps the session (3.5 s heartbeat rule) without restarting it
    QTest::qWait(1600);
    emit controller->bandwidthChanged();
    QObject* state = findItem(root.get(), QStringLiteral("bandwidthState"));
    QVERIFY(state);
    QTRY_VERIFY(state->property("text").toString().contains(QStringLiteral("does not support")));
}

// 2026-10-14: the Companion popup required a Vehicle (writeAllowed), so with the FC silent no link could be changed; its
// ports, bauds and lock text now all come from the CC.
void CompanionControllerTest::_testCompanionPopupWithoutVehicle()
{
    QQmlEngine engine;
    CompanionController* controller = nullptr;
    auto root = loadCompanionPage(engine, controller);
    QVERIFY(root && controller);
    controller->setSendHookForTest([](const mavlink_message_t&) { return true; });
    auto page = root->property("page").value<QObject*>();
    QVERIFY(page);
    auto blocked = page->findChild<QObject*>("companionBlockedText");
    QVERIFY(blocked);
    QVERIFY(!page->property("writeAllowed").toBool());
    QCOMPARE(blocked->property("text").toString(), QString("Mất kết nối tới Companion"));

    primeCompanionWithoutVehicle(*controller, 1);
    QTRY_VERIFY_WITH_TIMEOUT(page->property("writeAllowed").toBool(), 2000);
    QVERIFY(!blocked->property("visible").toBool());
    QObject* port0 = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((port0 = findItem(page, "portCombo0")), 2000);
    QVERIFY(port0->property("enabled").toBool());
    // The CC's boot list; the port another link uses is shown with its owner; "Tắt" switches the link off.
    QCOMPARE(comboTexts(port0), (QStringList{"/dev/ttyAMA4", "/dev/ttyAMA0 (dùng bởi SIYI)", "/dev/ttyUSB0", "Tắt"}));
    QCOMPARE(port0->property("currentText").toString(), QString("/dev/ttyAMA4"));
    auto baud0 = findItem(page, "baudCombo0");
    QVERIFY(baud0);
    QCOMPARE(comboTexts(baud0), controller->baudOptions(0));
    QCOMPARE(baud0->property("currentText").toString(), QString("921600"));

    // Choosing a port another link uses is refused with its owner's name; the selection stays.
    QMetaObject::invokeMethod(port0, "activated", Q_ARG(int, 1));
    QCOMPARE(page->property("localError").toString(), QString("Cổng đang được dùng bởi SIYI"));
    QCOMPARE(port0->property("currentText").toString(), QString("/dev/ttyAMA4"));

    // A port absent at boot stays shown as the current value only, marked.
    sendSerialLink(*controller, 1, 1, 2, "SIYI", "/dev/ttyUSB7", 115200);
    QObject* port1 = findItem(page, "portCombo1");
    QVERIFY(port1);
    QTRY_COMPARE_WITH_TIMEOUT(comboTexts(port1), (QStringList{"/dev/ttyAMA4 (dùng bởi FC)", "/dev/ttyAMA0", "/dev/ttyUSB0",
                                                              "/dev/ttyUSB7 (không có)", "Tắt"}), 2000);
    QCOMPARE(port1->property("currentText").toString(), QString("/dev/ttyUSB7 (không có)"));

    // Locked by the CC: the text is the CC's reason.
    sendSystemStatus(*controller, 1, CC_SYS_STATUS_CONFIG_LOCKED);
    QTRY_VERIFY_WITH_TIMEOUT(!page->property("writeAllowed").toBool(), 2000);
    QVERIFY(blocked->property("visible").toBool());
    QCOMPARE(blocked->property("text").toString(), controller->companionState()["lockReason"].toString());
    QVERIFY(!port0->property("enabled").toBool());

    // No parameter catalog: nothing is configurable, and the popup says so.
    sendSystemStatus(*controller, 1, 0);
    controller->_paramService->loadTemplate(QStringLiteral(":/missing/cc_parameters.json"));
    QTRY_COMPARE_WITH_TIMEOUT(blocked->property("text").toString(), QString("Chưa tải được danh sách tham số Companion"), 2000);
    QVERIFY(!page->property("writeAllowed").toBool());
}

// 2026-10-14: the popup had no Save and no way out of the reconnect wait, and Update stayed disabled for the current
// values, so a confirmed change could not be saved and a SAVE block could not be lifted from the UI.
void CompanionControllerTest::_testCompanionPopupSaveCancelAndRetry()
{
    QQmlEngine engine;
    CompanionController* controller = nullptr;
    auto root = loadCompanionPage(engine, controller);
    QVERIFY(root && controller);
    QList<mavlink_message_t> sent;
    controller->setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    auto page = root->property("page").value<QObject*>();
    QVERIFY(page);
    primeCompanionWithoutVehicle(*controller, 1);
    auto save = page->findChild<QObject*>("saveLinks");
    auto cancel = page->findChild<QObject*>("cancelCompanionWait");
    auto unsavedNote = page->findChild<QObject*>("linksUnsavedNote");
    QVERIFY(save && cancel && unsavedNote);
    QVERIFY(!save->property("visible").toBool());
    QVERIFY(!cancel->property("visible").toBool());

    QVERIFY(runLinkApplyToAccepted(*controller, 1, sent));
    QTRY_VERIFY_WITH_TIMEOUT(save->property("visible").toBool(), 2000);
    QVERIFY(unsavedNote->property("visible").toBool());
    QCOMPARE(unsavedNote->property("text").toString(), QString("Đã áp dụng, chưa lưu — reboot sẽ về cấu hình cũ"));
    QVERIFY(save->property("enabled").toBool());
    sent.clear();
    QMetaObject::invokeMethod(save, "clicked");
    QCOMPARE(countSent(sent, MAVLINK_MSG_ID_COMMAND_LONG), 1);
    mavlink_command_long_t cmd{};
    mavlink_msg_command_long_decode(&sent.last(), &cmd);
    QCOMPARE(cmd.command, static_cast<uint16_t>(MAV_CMD_THACO_SAVE_DEFAULT_CONFIG));
    QCOMPARE(cmd.param1, 1.0f);
    sendCommandAck(*controller, 1, MAV_CMD_THACO_SAVE_DEFAULT_CONFIG, MAV_RESULT_ACCEPTED, 0);
    QTRY_VERIFY_WITH_TIMEOUT(!save->property("visible").toBool(), 2000);

    // A links APPLY that lost the CC waits; "Hủy chờ" ends the wait, unconfirmed.
    sent.clear();
    QVERIFY(controller->setLinkConfig(0, "/dev/ttyAMA4", 921600));
    ackAllSets(*controller, 1, sent);
    QVERIFY(commandSent(sent));
    sendCommandAck(*controller, 1, MAV_CMD_THACO_APPLY_CONFIG, MAV_RESULT_IN_PROGRESS, 50);
    controller->setReceivedAgeForTest("heartbeat", 5000);
    QTRY_VERIFY_WITH_TIMEOUT(cancel->property("visible").toBool(), 2000);
    QMetaObject::invokeMethod(cancel, "clicked");
    QCOMPARE(controller->operationState("companion")["outcome"].toString(), QString("unconfirmed"));
    QTRY_VERIFY_WITH_TIMEOUT(!cancel->property("visible").toBool(), 2000);

    // The CC is back with the link as before: Update for the same values reads "Áp dụng lại" and sends the APPLY again.
    sendCompanionHeartbeat(*controller, 1);
    sendSerialLink(*controller, 1, 0, 2, "FC", "/dev/ttyUSB0", 57600);
    primeParam(*controller, 1, "CC_L0_PORT", QString("/dev/ttyUSB0")); // answers the re-read the heartbeat started
    primeParam(*controller, 1, "CC_L0_BAUD", 57600u);
    // 2026-10-19: a new CC session counts only values read in it (Task 5), so link 1 is re-read too, as the CC's LIST does.
    primeParam(*controller, 1, "CC_L1_PORT", QString("/dev/ttyAMA0"));
    primeParam(*controller, 1, "CC_L1_BAUD", 115200u);
    QVERIFY(findItem(page, "updateLink0"));
    QTRY_VERIFY_WITH_TIMEOUT(findItem(page, "updateLink0") && findItem(page, "updateLink0")->property("enabled").toBool(), 2000);
    auto update = findItem(page, "updateLink0");
    QVERIFY(update);
    QCOMPARE(update->property("text").toString(), QString("Áp dụng lại"));
    sent.clear();
    QMetaObject::invokeMethod(update, "clicked");
    QVERIFY(commandSent(sent));
    QCOMPARE(countSent(sent, MAVLINK_MSG_ID_PARAM_EXT_SET), 0);

    // The hotspot panel has its own Save, shown while the hotspot change is unsaved.
    auto saveHotspot = page->findChild<QObject*>("saveHotspot");
    QVERIFY(saveHotspot);
    QVERIFY(!saveHotspot->property("visible").toBool());
    controller->_unsaved["hotspot"] = true;
    emit controller->toolbarStateChanged();
    QVERIFY(saveHotspot->property("visible").toBool());
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

void CompanionControllerTest::_testConfigResponses()
{
    CompanionConfigService service;
    QSignalSpy changedSpy(&service, &CompanionConfigService::changed);
    service.setPendingForTest(17);
    mavlink_cc_config_ack_t ack{};
    ack.request_id = 17;
    ack.transaction_id = 42;
    ack.result = 1;
    copyField(ack.message, "Staging");
    mavlink_message_t message{};
    mavlink_msg_cc_config_ack_encode(7, 192, &message, &ack);
    service.processMessage(message, 7);
    QCOMPARE(changedSpy.count(), 0);
    mavlink_msg_cc_config_ack_encode(8, 191, &message, &ack);
    service.processMessage(message, 7);
    QCOMPARE(changedSpy.count(), 0);
    ack.request_id = 16;
    mavlink_msg_cc_config_ack_encode(7, 191, &message, &ack);
    service.processMessage(message, 7);
    QCOMPARE(changedSpy.count(), 0);
    ack.request_id = 17;
    mavlink_msg_cc_config_ack_encode(7, 191, &message, &ack);
    service.processMessage(message, 7);
    QCOMPARE(service.lastAck().value(QStringLiteral("transaction_id")).toUInt(), 42U);
    QCOMPARE(service.state(), CompanionConfigService::State::Staging);

    service.setPendingForTest(18, 42);
    mavlink_cc_config_value_t value{};
    value.request_id = 18;
    copyField(value.key, "verified.key");
    copyField(value.value, "verified.value");
    mavlink_msg_cc_config_value_encode(7, 191, &message, &value);
    service.processMessage(message, 7);
    QCOMPARE(service.lastValue().value(QStringLiteral("value")).toString(), QStringLiteral("verified.value"));

    mavlink_cc_config_status_t status{};
    status.transaction_id = 42;
    status.state = 3;
    status.progress = 75;
    mavlink_msg_cc_config_status_encode(7, 191, &message, &status);
    service.processMessage(message, 7);
    QCOMPARE(service.state(), CompanionConfigService::State::WaitingConfirm);
    QCOMPARE(service.progress(), 75);
    for (const auto& [wireState, expectedState] : {std::pair{quint8{1}, CompanionConfigService::State::Staging},
                                                   std::pair{quint8{2}, CompanionConfigService::State::Applying},
                                                   std::pair{quint8{4}, CompanionConfigService::State::Committed},
                                                   std::pair{quint8{5}, CompanionConfigService::State::Rollback}}) {
        status.state = wireState;
        mavlink_msg_cc_config_status_encode(7, 191, &message, &status);
        service.processMessage(message, 7);
        QCOMPARE(service.state(), expectedState);
    }
    status.state = 3;
    mavlink_msg_cc_config_status_encode(7, 191, &message, &status);
    service.processMessage(message, 7);
    status.transaction_id = 41;
    status.state = 4;
    mavlink_msg_cc_config_status_encode(7, 191, &message, &status);
    service.processMessage(message, 7);
    QCOMPARE(service.state(), CompanionConfigService::State::WaitingConfirm);
    service.reset();
    QCOMPARE(service.state(), CompanionConfigService::State::Unavailable);
    QVERIFY(service.lastAck().isEmpty());
}

void CompanionControllerTest::_testAiVisionStateInvariant()
{
    CompanionController controller;
    QSignalSpy changedSpy(&controller, &CompanionController::aiVisionControlChanged);

    QVERIFY(!controller.boundingBoxEnabled());
    QVERIFY(!controller.trackingEnabled());
    QVERIFY(!controller.followingEnabled());

    controller.setTrackingEnabled(true);
    controller.setFollowingEnabled(true);
    QCOMPARE(changedSpy.count(), 0);
    QVERIFY(!controller.trackingEnabled());
    QVERIFY(!controller.followingEnabled());

    controller.setBoundingBoxEnabled(true);
    QVERIFY(controller.boundingBoxEnabled());
    QVERIFY(!controller.trackingEnabled());
    QVERIFY(!controller.followingEnabled());

    controller.setTrackingEnabled(true);
    controller.setFollowingEnabled(true);
    QVERIFY(controller.trackingEnabled());
    QVERIFY(controller.followingEnabled());

    controller.setBoundingBoxEnabled(false);
    QVERIFY(!controller.boundingBoxEnabled());
    QVERIFY(!controller.trackingEnabled());
    QVERIFY(!controller.followingEnabled());
    QCOMPARE(changedSpy.count(), 4);
}

void CompanionControllerTest::_testAiVisionTrackPointGuards()
{
    CompanionController controller;

    QVERIFY(!controller.sendAiVisionTrackPoint(0.5, 0.5, 0.1));

    controller.setBoundingBoxEnabled(true);
    QVERIFY(!controller.sendAiVisionTrackPoint(-0.01, 0.5, 0.1));
    QVERIFY(!controller.sendAiVisionTrackPoint(1.01, 0.5, 0.1));
    QVERIFY(!controller.sendAiVisionTrackPoint(0.5, -0.01, 0.1));
    QVERIFY(!controller.sendAiVisionTrackPoint(0.5, 1.01, 0.1));
    QVERIFY(!controller.sendAiVisionTrackPoint(std::numeric_limits<double>::quiet_NaN(), 0.5, 0.1));
    QVERIFY(!controller.sendAiVisionTrackPoint(0.5, 0.5, -0.01));
    QVERIFY(!controller.sendAiVisionTrackPoint(0.5, 0.5, 1.01));
    QVERIFY(!controller.sendAiVisionTrackPoint(0.5, 0.5, 0.1));
}

// 2026-10-19: Bounding Box did nothing while the FC was down (no Vehicle): the AI Vision switches sent nothing to the CC.
void CompanionControllerTest::_testAiVisionWithoutVehicle()
{
    CompanionController controller;
    QList<mavlink_message_t> sent;
    controller.setSendHookForTest([&](const mavlink_message_t& m) { sent.append(m); return true; });
    sendCompanionHeartbeat(controller, 1);

    const auto lastControl = [&](mavlink_cc_ai_vision_control_t& control) {
        for (auto it = sent.crbegin(); it != sent.crend(); ++it) {
            if (it->msgid == MAVLINK_MSG_ID_CC_AI_VISION_CONTROL) {
                mavlink_msg_cc_ai_vision_control_decode(&*it, &control);
                return true;
            }
        }
        return false;
    };

    sent.clear();
    controller.setBoundingBoxEnabled(true);
    mavlink_cc_ai_vision_control_t control{};
    QVERIFY(lastControl(control));
    QCOMPARE(control.bounding_box, quint8(1));
    QCOMPARE(control.tracking, quint8(0));

    controller.setTrackingEnabled(true);
    QVERIFY(lastControl(control));
    QCOMPARE(control.tracking, quint8(1));

    sent.clear();
    QVERIFY(controller.sendAiVisionTrackPoint(0.25, 0.5, 0.03125));
    QCOMPARE(sent.size(), 1);
    QCOMPARE(sent.first().msgid, quint32(MAVLINK_MSG_ID_COMMAND_LONG));
    mavlink_command_long_t command{};
    mavlink_msg_command_long_decode(&sent.first(), &command);
    QCOMPARE(command.command, quint16(MAV_CMD_CAMERA_TRACK_POINT));
    QCOMPARE(command.target_system, quint8(1));
    QCOMPARE(command.target_component, quint8(191));
    QCOMPARE(command.param1, 0.25f);
}

// 2026-10-19: with the FC down and another UDP link open first, the control went (or would go) to the wrong link.
void CompanionControllerTest::_testAiVisionOnlyOnCompanionLinkWithoutVehicle()
{
    CompanionController controller;
    UdpPeerLink unrelated(this, QStringLiteral("Unrelated first UDP link"));
    UdpPeerLink companion(this, QStringLiteral("Companion without FC"));

    sendCompanionHeartbeat(controller, 42, companion.config->link());
    controller.setBoundingBoxEnabled(true);
    controller.setFollowingEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(companion.controls.size() >= 2, TestTimeout::mediumMs());
    mavlink_cc_ai_vision_control_t control{};
    mavlink_msg_cc_ai_vision_control_decode(&companion.controls.last(), &control);
    QCOMPARE(control.bounding_box, quint8(1));
    QCOMPARE(control.following, quint8(1));
    QVERIFY(unrelated.controls.isEmpty());
    QVERIFY(!MultiVehicleManager::instance()->activeVehicle());
}

namespace {
QStringList logLines(CompanionLogService* log, const QString& category)
{
    QStringList lines; // getLogHistory matches category OR message, so keep the category exact here
    for (const auto& item : log->getLogHistory(category))
        if (item.toMap()["category"].toString() == category) lines << item.toMap()["message"].toString();
    return lines;
}
}  // namespace

// 2026-10-19: a set left no trace outside the popup: no per-step log with timing for debugging.
void CompanionControllerTest::_testEveryOperationStepIsLogged()
{
    CompanionController controller;
    controller.setSendHookForTest([&](const mavlink_message_t&) { return true; });
    primeStreamMode(controller, 1, 1);
    primeParam(controller, 1, "CC_CAM_FPS", 30u);
    QVERIFY(controller.setStreamParameter("camera", "CC_CAM_FPS", 15));
    ackParamSet(controller, 1, "CC_CAM_FPS", 15u);
    const QStringList lines = logLines(controller._logService, "SET");
    QVERIFY2(lines.filter("PARAM_EXT_SET CC_CAM_FPS=15").size() == 1, qPrintable(lines.join('\n')));
    QVERIFY(lines.filter("PARAM_EXT_SET CC_CAM_FPS=15").first().contains("(+")); // each step carries its time since the op started
    QVERIFY(!lines.filter("ACK ACCEPTED").isEmpty());
    const auto summary = lines.filter("CC_CAM_FPS=30->15");
    QCOMPARE(summary.size(), 1);
    QVERIFY(summary.first().contains(" ok first_ack="));
    QVERIFY(summary.first().contains("ms total="));
}

// 2026-10-19: a stream SET without a known value must log an explicit "?" for it, not "" or 0.
void CompanionControllerTest::_testUnknownPreviousValueIsLoggedAsQuestionMark()
{
    CompanionController controller;
    controller.setSendHookForTest([&](const mavlink_message_t&) { return true; });
    primeStreamMode(controller, 1, 1); // CC_CAM_FPS is not read
    QVERIFY(!controller._paramService->confirmedValue("CC_CAM_FPS").isValid());
    QVERIFY(controller.setStreamParameter("camera", "CC_CAM_FPS", 15));
    ackParamSet(controller, 1, "CC_CAM_FPS", 15u);
    const QStringList lines = logLines(controller._logService, "SET");
    QVERIFY2(lines.filter("CC_CAM_FPS=?->15").size() == 1, qPrintable(lines.join('\n')));
}

// 2026-10-19: the new log lines must never carry the hotspot password, whatever the outcome.
void CompanionControllerTest::_testLogNeverContainsTheHotspotPassword()
{
    CompanionController controller;
    controller.setSendHookForTest([&](const mavlink_message_t&) { return true; });
    primeCompanionWithoutVehicle(controller, 1);
    primeHotspot(controller, 1); // CC_AP_PASS = "old-password"
    QVERIFY(controller.applyHotspot({{"ssid", "NewAP"}, {"password", "s3cret-new-pass"}, {"ipCidr", "192.168.10.1/24"}, {"dhcpEnabled", true}}));
    ackParamSet(controller, 1, "CC_AP_SSID", QString("NewAP"));
    ackParamSet(controller, 1, "CC_AP_PASS", QString(), PARAM_ACK_FAILED); // the CC refuses: the rollback follows
    ackParamSet(controller, 1, "CC_AP_SSID", QString("Existing"));
    const auto history = controller._logService->getLogHistory("ALL");
    QVERIFY(!history.isEmpty());
    for (const auto& item : history) {
        const QString text = item.toMap()["message"].toString();
        QVERIFY2(!text.contains("s3cret-new-pass") && !text.contains("old-password"), qPrintable(text));
    }
    // the SET level did log the secret's step, hidden on both sides of the summary
    const QString all = logLines(controller._logService, "SET").join('\n');
    QVERIFY2(all.contains("PARAM_EXT_SET CC_AP_PASS=<hidden>"), qPrintable(all));
    QVERIFY2(all.contains("CC_AP_PASS=<hidden>-><hidden>") && all.contains("error:"), qPrintable(all));
}

// 2026-10-19: an op that ended unconfirmed or was replaced left no summary line in the log.
void CompanionControllerTest::_testUnconfirmedOperationLogsOneSummary()
{
    CompanionController controller;
    controller.setSendHookForTest([&](const mavlink_message_t&) { return true; });
    primeStreamMode(controller, 1, 1);
    primeParam(controller, 1, "CC_CAM_FPS", 30u);
    QVERIFY(controller.setStreamParameter("camera", "CC_CAM_FPS", 15));
    controller.setAckTimedOut(); // the CC stays silent: the op ends unconfirmed
    const QStringList lines = logLines(controller._logService, "SET").filter("CC_CAM_FPS=30->15");
    QVERIFY2(lines.size() == 1 && lines.first().contains(" unconfirmed"), qPrintable(lines.join('\n')));
}

// 2026-10-19: an op that ended unconfirmed or was replaced left no summary line in the log.
void CompanionControllerTest::_testReplacedModeOperationLogsOneSummary()
{
    CompanionController controller;
    controller.setSendHookForTest([&](const mavlink_message_t&) { return true; });
    primeStreamMode(controller, 1, 1);
    QVERIFY(controller.setStreamMode(2));
    QVERIFY(controller.setStreamMode(0)); // replaces the pending mode 2
    QStringList lines = logLines(controller._logService, "SET").filter(" first_ack=");
    QCOMPARE(lines.filter(" replaced").size(), 1);
    QCOMPARE(lines.filter("CC_STREAM_MODE=1->2").size(), 1);
    controller.setAckTimedOut(); // the new op gets its own line later
    lines = logLines(controller._logService, "SET").filter(" first_ack=");
    QCOMPARE(lines.size(), 2);
    QVERIFY2(lines.last().contains(" unconfirmed"), qPrintable(lines.join('\n')));
}

// 2026-10-19: a replace whose SET could not be sent logged "replaced" for an op that went on.
void CompanionControllerTest::_testFailedReplaceLogsNoReplacedSummary()
{
    CompanionController controller;
    controller.setSendHookForTest([&](const mavlink_message_t&) { return true; });
    primeStreamMode(controller, 1, 1);
    QVERIFY(controller.setStreamMode(2));
    controller.setSendHookForTest([](const mavlink_message_t&) { return false; });
    QVERIFY(!controller.setStreamMode(0)); // refused: the replacing SET cannot be sent
    const QStringList lines = logLines(controller._logService, "SET").filter(" first_ack=");
    QVERIFY2(lines.filter(" replaced").isEmpty(), qPrintable(lines.join('\n')));
    QVERIFY(controller.modeOperationPending());
}

void CompanionVehicleLifecycleTest::_testSingleMessageDispatch()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    QVERIFY(vehicle());
    const auto link = vehicle()->vehicleLinkManager()->primaryLink().lock();
    QVERIFY(link);
    QSignalSpy logged(&controller, &CompanionController::mavlinkLogMessage);
    mavlink_message_t message{};
    mavlink_msg_statustext_pack(vehicle()->id(), 191, &message, MAV_SEVERITY_INFO, "VIS: one message", 0, 0);
    // The protocol forwards this to Vehicle, so emitting its signal again would inject a second message.
    emit MAVLinkProtocol::instance() -> messageReceived(link.get(), message);
    QCOMPARE(logged.count(), 1);
}

void CompanionVehicleLifecycleTest::_testAiVisionTransmission()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    QVERIFY(vehicle());
    sendCompanionHeartbeat(controller, vehicle()->id(), mockLink());  // the AI Vision state goes through the CC session

    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_CC_AI_VISION_CONTROL) >= 1, 3000);

    const auto verifyLastState = [this](quint8 boundingBox, quint8 tracking, quint8 following) {
        mavlink_message_t message{};
        QVERIFY(mockLink()->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_CC_AI_VISION_CONTROL, message));
        mavlink_cc_ai_vision_control_t control{};
        mavlink_msg_cc_ai_vision_control_decode(&message, &control);
        QCOMPARE(control.bounding_box, boundingBox);
        QCOMPARE(control.tracking, tracking);
        QCOMPARE(control.following, following);
    };

    verifyLastState(0, 0, 0);

    mockLink()->clearReceivedMavlinkMessageCounts();
    controller.setBoundingBoxEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_CC_AI_VISION_CONTROL) >= 1, 1000);
    verifyLastState(1, 0, 0);

    mockLink()->clearReceivedMavlinkMessageCounts();
    controller.setTrackingEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_CC_AI_VISION_CONTROL) >= 1, 1000);
    verifyLastState(1, 1, 0);

    mockLink()->clearReceivedMavlinkMessageCounts();
    controller.setFollowingEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_CC_AI_VISION_CONTROL) >= 1, 1000);
    verifyLastState(1, 1, 1);

    mockLink()->clearReceivedMavlinkMessageCounts();
    controller.setBoundingBoxEnabled(false);
    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_CC_AI_VISION_CONTROL) >= 1, 1000);
    verifyLastState(0, 0, 0);

    mockLink()->clearReceivedMavlinkMessageCounts();
    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_CC_AI_VISION_CONTROL) >= 2, 2500);
    verifyLastState(0, 0, 0);

    _disconnectMockLink();
    QVERIFY(!controller.vehicleAvailable());
    controller.setBoundingBoxEnabled(true);
    controller.setFollowingEnabled(true);
    QCOMPARE(controller.boundingBoxEnabled(), true);
    QCOMPARE(controller.trackingEnabled(), false);
    QCOMPARE(controller.followingEnabled(), true);

    _connectMockLinkNoInitialConnectSequence();
    sendCompanionHeartbeat(controller, vehicle()->id(), mockLink());
    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_CC_AI_VISION_CONTROL) >= 1, 3000);
    verifyLastState(1, 0, 1);
}

// Ported from be5e437: the control follows the CC heartbeat link, never the FC's link, and stops when that link is lost.
void CompanionVehicleLifecycleTest::_testAiVisionUsesCompanionLinkWithVehicle()
{
    CompanionController controller;
    _connectMockLinkNoInitialConnectSequence();
    QVERIFY(vehicle());
    UdpPeerLink companion(this, QStringLiteral("Companion separate from FC"));
    QVERIFY(companion.config->link() != vehicle()->vehicleLinkManager()->primaryLink().lock().get());

    sendCompanionHeartbeat(controller, vehicle()->id(), companion.config->link());
    mockLink()->clearReceivedMavlinkMessageCounts();
    controller.setBoundingBoxEnabled(true);
    controller.setFollowingEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(companion.controls.size() >= 2, TestTimeout::mediumMs());
    mavlink_cc_ai_vision_control_t control{};
    mavlink_msg_cc_ai_vision_control_decode(&companion.controls.last(), &control);
    QCOMPARE(control.bounding_box, quint8(1));
    QCOMPARE(control.following, quint8(1));
    QCOMPARE(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_CC_AI_VISION_CONTROL), 0);

    // Losing the CC link must not redirect the control to the still-connected FC link.
    LinkManager::instance()->disconnectLink(companion.config->link());
    QTRY_VERIFY_WITH_TIMEOUT(!companion.config->link(), TestTimeout::shortMs());
    QSignalSpy ticks(&controller._aiVisionControlTimer, &QTimer::timeout);
    controller._aiVisionControlTimer.start(20);
    QTRY_VERIFY_WITH_TIMEOUT(ticks.count() >= 2, TestTimeout::shortMs());
    controller._aiVisionControlTimer.stop();
    QCOMPARE(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_CC_AI_VISION_CONTROL), 0);
}

void CompanionVehicleLifecycleTest::_testAiVisionTrackPointTransmission()
{
    CompanionController controller;

    controller.setBoundingBoxEnabled(true);
    {
        Vehicle offlineVehicle(MAV_AUTOPILOT_PX4, MAV_TYPE_QUADROTOR);
        controller._setActiveVehicle(&offlineVehicle);
        QVERIFY(!offlineVehicle.vehicleLinkManager()->primaryLink().lock());
        QVERIFY(!controller.sendAiVisionTrackPoint(0.5, 0.5, 0.1));
        controller._setActiveVehicle(nullptr);
    }
    controller.setBoundingBoxEnabled(false);

    _connectMockLinkNoInitialConnectSequence();
    QVERIFY(vehicle());

    mockLink()->clearReceivedMavCommandCounts();
    QVERIFY(!controller.sendAiVisionTrackPoint(0.5, 0.5, 0.1));
    QCOMPARE(mockLink()->receivedMavCommandCount(MAV_CMD_CAMERA_TRACK_POINT, 191), 0);

    sendCompanionHeartbeat(controller, vehicle()->id(), mockLink());
    controller.setBoundingBoxEnabled(true);
    mockLink()->clearReceivedMavCommandCounts();
    mockLink()->clearReceivedMavlinkMessageCounts();

    constexpr float kFloatTolerance = 1e-6F;
    QVERIFY(controller.sendAiVisionTrackPoint(0.25, 0.5, 0.03125));
    QTRY_COMPARE_WITH_TIMEOUT(mockLink()->receivedMavCommandCount(MAV_CMD_CAMERA_TRACK_POINT, 191), 1, 1000);
    QCOMPARE(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_COMMAND_LONG), 1);

    mavlink_message_t message{};
    QVERIFY(mockLink()->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_COMMAND_LONG, message));
    QCOMPARE(message.msgid, MAVLINK_MSG_ID_COMMAND_LONG);
    QCOMPARE(message.sysid, MAVLinkProtocol::instance()->getSystemId());
    QCOMPARE(message.compid, MAVLinkProtocol::getComponentId());
    mavlink_command_long_t command{};
    mavlink_msg_command_long_decode(&message, &command);
    QCOMPARE(command.command, MAV_CMD_CAMERA_TRACK_POINT);
    QCOMPARE(command.target_system, vehicle()->id());
    QCOMPARE(command.target_component, 191);
    QCOMPARE(command.confirmation, 0);
    QVERIFY(qAbs(command.param1 - 0.25F) <= kFloatTolerance);
    QVERIFY(qAbs(command.param2 - 0.5F) <= kFloatTolerance);
    QVERIFY(qAbs(command.param3 - 0.03125F) <= kFloatTolerance);
    QVERIFY(qAbs(command.param4) <= kFloatTolerance);
    QVERIFY(qAbs(command.param5) <= kFloatTolerance);
    QVERIFY(qAbs(command.param6) <= kFloatTolerance);
    QVERIFY(qAbs(command.param7) <= kFloatTolerance);

    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_CC_AI_VISION_CONTROL) >= 4, 5500);
    QCOMPARE(mockLink()->receivedMavCommandCount(MAV_CMD_CAMERA_TRACK_POINT, 191), 1);

    mockLink()->clearReceivedMavCommandCounts();
    QVERIFY(!controller.sendAiVisionTrackPoint(-0.01, 0.5, 0.1));
    QVERIFY(!controller.sendAiVisionTrackPoint(1.01, 0.5, 0.1));
    QVERIFY(!controller.sendAiVisionTrackPoint(0.5, -0.01, 0.1));
    QVERIFY(!controller.sendAiVisionTrackPoint(0.5, 1.01, 0.1));
    QCOMPARE(mockLink()->receivedMavCommandCount(MAV_CMD_CAMERA_TRACK_POINT, 191), 0);

    controller.setBoundingBoxEnabled(false);
    mockLink()->clearReceivedMavCommandCounts();
    QVERIFY(!controller.sendAiVisionTrackPoint(0.25, 0.5, 0.03125));
    QCOMPARE(mockLink()->receivedMavCommandCount(MAV_CMD_CAMERA_TRACK_POINT, 191), 0);
}
