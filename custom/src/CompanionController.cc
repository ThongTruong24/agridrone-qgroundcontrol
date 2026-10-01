#include "CompanionController.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "CompanionLinksService.h"
#include "CompanionLogService.h"
#include "MAVLinkProtocol.h"
#include "MultiVehicleManager.h"
#include "Vehicle.h"
#include "VehicleLinkManager.h"

namespace {
constexpr quint8 kCompanionComponentId = 191;
constexpr quint16 kApplyConfigCommand = 44011;
constexpr quint16 kSaveDefaultConfigCommand = 44010;
constexpr int kConfirmationTimeoutMs = 6000;

template <std::size_t Size>
QString fixedMavlinkString(const char (&text)[Size])
{
    const auto end = std::find(text, text + Size, '\0');
    return QString::fromUtf8(text, static_cast<qsizetype>(end - text));
}

QVariantMap emptyLinks()
{
    return {
        {QStringLiteral("fc_baudrate"), qulonglong{0}},
        {QStringLiteral("siyi_baudrate"), qulonglong{0}},
        {QStringLiteral("fc_bytes_rx"), qulonglong{0}},
        {QStringLiteral("fc_bytes_tx"), qulonglong{0}},
        {QStringLiteral("fc_status"), 0},
        {QStringLiteral("siyi_status"), 0},
        {QStringLiteral("transport_type"), 0},
        {QStringLiteral("available_ports"), QString()},
        {QStringLiteral("fc_rx_rate"), 0.0},
        {QStringLiteral("fc_tx_rate"), 0.0},
        {QStringLiteral("fc_rx_loss"), 0.0},
        {QStringLiteral("fc_tx_err"), qulonglong{0}},
        {QStringLiteral("siyi_rx_rate"), 0.0},
        {QStringLiteral("siyi_tx_rate"), 0.0},
        {QStringLiteral("siyi_rx_loss"), 0.0},
        {QStringLiteral("siyi_tx_err"), qulonglong{0}},
        {QStringLiteral("siyi_bytes_rx"), qulonglong{0}},
        {QStringLiteral("siyi_bytes_tx"), qulonglong{0}},
        {QStringLiteral("fc_port"), QString()},
        {QStringLiteral("siyi_port"), QString()},
    };
}

QVariantMap emptyCamera()
{
    return {
        {QStringLiteral("video_width"), 0},
        {QStringLiteral("video_height"), 0},
        {QStringLiteral("rotation"), 0},
        {QStringLiteral("depth_width"), 0},
        {QStringLiteral("depth_height"), 0},
        {QStringLiteral("bitrate_kbps"), 0},
        {QStringLiteral("bitrate_max_kbps"), 0},
        {QStringLiteral("vbv_buffer_kb"), 0},
        {QStringLiteral("video_fps"), 0},
        {QStringLiteral("depth_fps"), 0},
        {QStringLiteral("profile_mode"), 0},
        {QStringLiteral("enable_emitter"), 0},
        {QStringLiteral("camera_type"), QString()},
        {QStringLiteral("serial_number"), QString()},
        {QStringLiteral("codec"), QString()},
        {QStringLiteral("encoder_mode"), QString()},
        {QStringLiteral("rtsp_url_qgc"), QString()},
        {QStringLiteral("rtsp_url_controller"), QString()},
        {QStringLiteral("usb_speed_mode"), 0},
    };
}

QVariantMap emptyNetwork()
{
    return {
        {QStringLiteral("ap_channel"), 0},           {QStringLiteral("ap_ieee80211n"), 0},
        {QStringLiteral("ap_wmm_enabled"), 0},       {QStringLiteral("ap_wpa"), 0},
        {QStringLiteral("ap_client_count"), 0},      {QStringLiteral("wlan0_dhcp"), 0},
        {QStringLiteral("dnsmasq_status"), 0},       {QStringLiteral("eth0_ip"), QString()},
        {QStringLiteral("wlan0_ip"), QString()},     {QStringLiteral("ap_ip"), QString()},
        {QStringLiteral("eth0_netmask"), QString()}, {QStringLiteral("ap_status"), 0},
        {QStringLiteral("ap_ssid"), QString()},      {QStringLiteral("ap_wpa_passphrase"), QString()},
        {QStringLiteral("ap_key_mgmt"), QString()},  {QStringLiteral("ap_hw_mode"), QString()},
    };
}

QVariantMap emptyVision()
{
    return {
        {QStringLiteral("confidence_thresh"), 0.0},
        {QStringLiteral("inference_fps"), 0.0},
        {QStringLiteral("input_width"), 0},
        {QStringLiteral("input_height"), 0},
        {QStringLiteral("video_fps"), 0},
        {QStringLiteral("detections_count"), 0},
        {QStringLiteral("status_flags"), 0},
        {QStringLiteral("model_name"), QString()},
        {QStringLiteral("input_source"), QString()},
    };
}

bool isCcTelemetryMessage(const mavlink_message_t& message)
{
    switch (message.msgid) {
        case MAVLINK_MSG_ID_CC_TELEMETRY_LINKS:
        case MAVLINK_MSG_ID_CC_TELEMETRY_CAMERA:
        case MAVLINK_MSG_ID_CC_TELEMETRY_NETWORK:
        case MAVLINK_MSG_ID_CC_TELEMETRY_VISION:
        case MAVLINK_MSG_ID_CC_TELEMETRY_SYSTEM:
            return true;
        default:
            return false;
    }
}
}  // namespace

CompanionController::CompanionController(QObject* parent)
    : QObject(parent), _links(emptyLinks()), _camera(emptyCamera()), _network(emptyNetwork()), _vision(emptyVision())
{
    _logService = new CompanionLogService(this);
    (void) connect(_logService, &CompanionLogService::logMessageAdded, this, &CompanionController::mavlinkLogMessage);
    _clock.start();
    _staleTimer.setInterval(kStaleCheckIntervalMs);
    _staleTimer.setSingleShot(false);
    (void) connect(&_staleTimer, &QTimer::timeout, this, &CompanionController::_updateStaleStates);
    _staleTimer.start();
    _configTimer.setSingleShot(true);
    _configTimer.setInterval(3500);
    (void) connect(&_configTimer, &QTimer::timeout, this, &CompanionController::_configTimedOut);
    _confirmationTimer.setSingleShot(true);
    _confirmationTimer.setInterval(kConfirmationTimeoutMs);
    (void) connect(&_confirmationTimer, &QTimer::timeout, this, &CompanionController::_confirmationTimedOut);
    (void) connect(&_configService, &CompanionConfigService::changed, this, &CompanionController::configStatusChanged);

    MultiVehicleManager* const multiVehicleManager = MultiVehicleManager::instance();
    (void) connect(multiVehicleManager, &MultiVehicleManager::activeVehicleChanged, this,
                   &CompanionController::_setActiveVehicle);
    _setActiveVehicle(multiVehicleManager->activeVehicle());
}

void CompanionController::_setActiveVehicle(Vehicle* vehicle)
{
    if (_activeVehicle == vehicle) {
        return;
    }

    const bool wasAvailable = vehicleAvailable();
    QObject::disconnect(_vehicleMessageConnection);
    QObject::disconnect(_vehicleDestroyedConnection);
    _activeVehicle = vehicle;
    _resetTelemetry();
    _configService.reset();
    _configTimer.stop();
    _confirmationTimer.stop();
    _pendingCommand = 0;
    _waitingTelemetry = false;
    _setConfigStatus(QStringLiteral("Idle"), QString());
    ++_vehicleEpoch;
    emit vehicleEpochChanged();

    if (_activeVehicle) {
        _vehicleMessageConnection = connect(_activeVehicle, &Vehicle::mavlinkMessageReceived, this,
                                            &CompanionController::_mavlinkMessageReceived, Qt::UniqueConnection);
        _vehicleDestroyedConnection = connect(_activeVehicle, &QObject::destroyed, this, [this]() {
            _activeVehicle = nullptr;
            _resetTelemetry();
            _configService.reset();
            _configTimer.stop();
            _confirmationTimer.stop();
            _pendingCommand = 0;
            _waitingTelemetry = false;
            _setConfigStatus(QStringLiteral("Idle"), QString());
            ++_vehicleEpoch;
            emit vehicleEpochChanged();
            emit vehicleAvailableChanged();
        });
    }

    if (wasAvailable != vehicleAvailable()) {
        emit vehicleAvailableChanged();
    }
}

void CompanionController::_mavlinkMessageReceived(const mavlink_message_t& message)
{
    if (message.msgid == MAVLINK_MSG_ID_THACO_EXTERNAL_XYZ_TRIGGER) {
        if (_activeVehicle && message.sysid == _activeVehicle->id()) {
            mavlink_thaco_external_xyz_trigger_t trigger{};
            mavlink_msg_thaco_external_xyz_trigger_decode(&message, &trigger);
            _lastTriggerId = trigger.trigger_id;
            _lastTriggerTimeBootMs = trigger.time_boot_ms;
            emit missionChanged();
            _markReceived(_missionState, &CompanionController::missionStatusChanged);
            _logService->logMavlink(
                QStringLiteral("MISSION"), QStringLiteral("RX"),
                QStringLiteral("MISSION: XYZ trigger %1 at %2 ms").arg(trigger.trigger_id).arg(trigger.time_boot_ms));
        }
        return;
    }
    if (message.msgid == MAVLINK_MSG_ID_STATUSTEXT && _activeVehicle && message.sysid == _activeVehicle->id() &&
        message.compid == kCompanionComponentId) {
        mavlink_statustext_t status{};
        mavlink_msg_statustext_decode(&message, &status);
        const QString statusText = fixedMavlinkString(status.text);
        if (!statusText.isEmpty()) {
            _logService->logMavlink(QStringLiteral("ALL"), QStringLiteral("RX"), statusText, status.severity);
        }
        return;
    }
    if (message.msgid == MAVLINK_MSG_ID_CC_CONFIG_ACK || message.msgid == MAVLINK_MSG_ID_CC_CONFIG_VALUE ||
        message.msgid == MAVLINK_MSG_ID_CC_CONFIG_STATUS) {
        if (_activeVehicle) {
            _configService.processMessage(message, _activeVehicle->id());
        }
        return;
    }
    if (message.msgid == MAVLINK_MSG_ID_COMMAND_ACK) {
        _processCommandAck(message);
        return;
    }
    if (!isCcTelemetryMessage(message) || !_acceptSource(message, true)) {
        return;
    }
    _processMessage(message);
}

QStringList CompanionController::availablePorts() const
{
    QStringList ports;
    const QStringList reported =
        _links.value(QStringLiteral("available_ports")).toString().split(',', Qt::SkipEmptyParts);
    for (const QString& candidate : reported + QStringList{_links.value(QStringLiteral("fc_port")).toString(),
                                                           _links.value(QStringLiteral("siyi_port")).toString()}) {
        const QString port = candidate.trimmed();
        if (!port.isEmpty() && !ports.contains(port)) {
            ports.append(port);
        }
    }
    return ports;
}

QVariantList CompanionController::getLogHistory(const QString& category) const
{
    return _logService->getLogHistory(category);
}

void CompanionController::clearLogHistory(const QString& category)
{
    _logService->clearLogHistory(category);
}

bool CompanionController::logMatchesFilter(const QString& filter, const QString& category, const QString& message) const
{
    return CompanionLogService::matchesFilter(filter, category, message);
}

void CompanionController::_setConfigStatus(const QString& status, const QString& message)
{
    if (_configStatus == status && _configMessage == message) {
        return;
    }
    _configStatus = status;
    _configMessage = message;
    emit configStatusChanged();
}

bool CompanionController::_sendCommand(quint16 command)
{
    if (!_activeVehicle) {
        return false;
    }
    const auto link = _activeVehicle->vehicleLinkManager()->primaryLink().lock();
    if (!link) {
        return false;
    }
    mavlink_command_long_t payload{};
    payload.target_system = _activeVehicle->id();
    payload.target_component = kCompanionComponentId;
    payload.command = command;
    payload.param1 = 1.0F;                                          // UART / links subsystem
    payload.param2 = command == kApplyConfigCommand ? 1.0F : 0.0F;  // Apply immediately
    mavlink_message_t message{};
    mavlink_msg_command_long_encode_chan(MAVLinkProtocol::instance()->getSystemId(), MAVLinkProtocol::getComponentId(),
                                         link->mavlinkChannel(), &message, &payload);
    return _activeVehicle->sendMessageOnLinkThreadSafe(link.get(), message);
}

void CompanionController::applyLinksConfig(const QString& fcPort, int fcBaud, const QString& siyiPort, int siyiBaud)
{
    if (_pendingCommand || _waitingTelemetry) {
        return;
    }
    if (!_activeVehicle || !linksReceived() || linksStale() || fcPort.trimmed().isEmpty() ||
        siyiPort.trimmed().isEmpty() || fcBaud <= 0 || siyiBaud <= 0) {
        _setConfigStatus(QStringLiteral("Failed"), QStringLiteral("No fresh links telemetry or invalid UART settings"));
        return;
    }
    const QByteArray fcBytes = fcPort.trimmed().toUtf8();
    const QByteArray siyiBytes = siyiPort.trimmed().toUtf8();
    if (fcBytes.size() > 15 || siyiBytes.size() > 15) {
        _setConfigStatus(QStringLiteral("Failed"), QStringLiteral("UART port must fit within 15 UTF-8 bytes"));
        return;
    }
    const auto link = _activeVehicle->vehicleLinkManager()->primaryLink().lock();
    if (!link) {
        _setConfigStatus(QStringLiteral("Failed"), QStringLiteral("No active MAVLink link"));
        return;
    }
    mavlink_cc_telemetry_links_t payload = _linksPacket;
    payload.fc_baudrate = static_cast<quint32>(fcBaud);
    payload.siyi_baudrate = static_cast<quint32>(siyiBaud);
    std::memset(payload.fc_port, 0, sizeof(payload.fc_port));
    std::memset(payload.siyi_port, 0, sizeof(payload.siyi_port));
    std::memcpy(payload.fc_port, fcBytes.constData(), std::min<qsizetype>(fcBytes.size(), sizeof(payload.fc_port) - 1));
    std::memcpy(payload.siyi_port, siyiBytes.constData(),
                std::min<qsizetype>(siyiBytes.size(), sizeof(payload.siyi_port) - 1));
    if (!CompanionLinksService::sendLegacyConfig(_activeVehicle, payload)) {
        _setConfigStatus(QStringLiteral("Failed"), QStringLiteral("Could not send UART configuration"));
        return;
    }
    _pendingCommand = kApplyConfigCommand;
    _expectedFcPort = fcPort.trimmed();
    _expectedSiyiPort = siyiPort.trimmed();
    _expectedFcBaud = fcBaud;
    _expectedSiyiBaud = siyiBaud;
    if (!_sendCommand(kApplyConfigCommand)) {
        _pendingCommand = 0;
        _setConfigStatus(QStringLiteral("Failed"), QStringLiteral("Could not send Apply command"));
        return;
    }
    _logService->logMavlink(
        QStringLiteral("UART"), QStringLiteral("TX"),
        QStringLiteral("Apply FC %1 @ %2, SIYI %3 @ %4").arg(fcPort).arg(fcBaud).arg(siyiPort).arg(siyiBaud));
    _setConfigStatus(QStringLiteral("Applying"), QStringLiteral("Waiting for Companion ACK"));
    _configTimer.start();
}

void CompanionController::saveLinksConfig()
{
    if (_pendingCommand || _waitingTelemetry) {
        return;
    }
    if (!_activeVehicle || !linksReceived() || linksStale()) {
        _setConfigStatus(QStringLiteral("Failed"), QStringLiteral("No fresh links telemetry for Save"));
        return;
    }
    _pendingCommand = kSaveDefaultConfigCommand;
    if (!_sendCommand(kSaveDefaultConfigCommand)) {
        _pendingCommand = 0;
        _setConfigStatus(QStringLiteral("Failed"), QStringLiteral("Could not send Save command"));
        return;
    }
    _logService->logMavlink(QStringLiteral("UART"), QStringLiteral("TX"), QStringLiteral("Save UART defaults"));
    _setConfigStatus(QStringLiteral("Applying"), QStringLiteral("Waiting for Save ACK"));
    _configTimer.start();
}

void CompanionController::_processCommandAck(const mavlink_message_t& message)
{
    if (!_pendingCommand || !_activeVehicle || message.sysid != _activeVehicle->id() ||
        message.compid != kCompanionComponentId) {
        return;
    }
    mavlink_command_ack_t ack{};
    mavlink_msg_command_ack_decode(&message, &ack);
    if (ack.command != _pendingCommand ||
        (ack.target_system != 0 && ack.target_system != MAVLinkProtocol::instance()->getSystemId()) ||
        (ack.target_component != 0 && ack.target_component != MAVLinkProtocol::getComponentId())) {
        return;
    }
    if (ack.result == MAV_RESULT_IN_PROGRESS) {
        _setConfigStatus(QStringLiteral("Applying"), QStringLiteral("Companion reports command in progress"));
        _configTimer.start();
        return;
    }
    const quint16 completedCommand = _pendingCommand;
    _configTimer.stop();
    _pendingCommand = 0;
    emit commandAckReceived(ack.command, ack.result);
    if (ack.result == MAV_RESULT_ACCEPTED) {
        if (completedCommand == kApplyConfigCommand) {
            _waitingTelemetry = true;
            _setConfigStatus(QStringLiteral("WaitingTelemetry"),
                             QStringLiteral("ACK accepted; waiting for UART telemetry"));
            _confirmationTimer.start();
        } else {
            _setConfigStatus(QStringLiteral("Success"), QStringLiteral("Companion saved UART defaults"));
        }
        _logService->logMavlink(QStringLiteral("UART"), QStringLiteral("RX"), QStringLiteral("Command accepted"));
    } else {
        _setConfigStatus(QStringLiteral("Failed"), QStringLiteral("Companion rejected command (%1)").arg(ack.result));
        _logService->logMavlink(QStringLiteral("UART"), QStringLiteral("RX"), _configMessage, 3);
    }
}

void CompanionController::_checkTelemetryConfirmation()
{
    if (!_waitingTelemetry || !linksReceived() || linksStale()) {
        return;
    }
    if (_links.value(QStringLiteral("fc_port")).toString() != _expectedFcPort ||
        _links.value(QStringLiteral("fc_baudrate")).toInt() != _expectedFcBaud ||
        _links.value(QStringLiteral("siyi_port")).toString() != _expectedSiyiPort ||
        _links.value(QStringLiteral("siyi_baudrate")).toInt() != _expectedSiyiBaud) {
        return;
    }
    _confirmationTimer.stop();
    _waitingTelemetry = false;
    _setConfigStatus(QStringLiteral("Success"), QStringLiteral("UART configuration confirmed by telemetry"));
}

void CompanionController::_configTimedOut()
{
    if (!_pendingCommand) {
        return;
    }
    _pendingCommand = 0;
    _setConfigStatus(QStringLiteral("Timeout"), QStringLiteral("No Companion ACK; retry is available"));
    _logService->logMavlink(QStringLiteral("UART"), QStringLiteral("RX"), _configMessage, 3);
}

void CompanionController::_confirmationTimedOut()
{
    if (!_waitingTelemetry) {
        return;
    }
    _waitingTelemetry = false;
    _setConfigStatus(QStringLiteral("Timeout"),
                     QStringLiteral("UART telemetry did not confirm the applied configuration"));
    _logService->logMavlink(QStringLiteral("UART"), QStringLiteral("RX"), _configMessage, 3);
}

bool CompanionController::_acceptSource(const mavlink_message_t& message, bool requireActiveVehicle)
{
    if (message.compid != kCompanionComponentId) {
        return false;
    }
    if (requireActiveVehicle && (!_activeVehicle || (message.sysid != _activeVehicle->id()))) {
        return false;
    }

    if (_sourceComponentId < 0) {
        _sourceSystemId = message.sysid;
        _sourceComponentId = message.compid;
        emit sourceChanged();
        return true;
    }

    return (message.sysid == _sourceSystemId) && (message.compid == _sourceComponentId);
}

void CompanionController::_processMessage(const mavlink_message_t& message)
{
    switch (message.msgid) {
        case MAVLINK_MSG_ID_CC_TELEMETRY_LINKS: {
            mavlink_msg_cc_telemetry_links_decode(&message, &_linksPacket);
            _links = CompanionLinksService::decode(message);
            emit linksChanged();
            _markReceived(_linksState, &CompanionController::linksStatusChanged);
            _checkTelemetryConfirmation();
            break;
        }
        case MAVLINK_MSG_ID_CC_TELEMETRY_CAMERA: {
            mavlink_cc_telemetry_camera_t packet{};
            mavlink_msg_cc_telemetry_camera_decode(&message, &packet);
            _camera = {
                {QStringLiteral("video_width"), packet.video_width},
                {QStringLiteral("video_height"), packet.video_height},
                {QStringLiteral("rotation"), packet.rotation},
                {QStringLiteral("depth_width"), packet.depth_width},
                {QStringLiteral("depth_height"), packet.depth_height},
                {QStringLiteral("bitrate_kbps"), packet.bitrate_kbps},
                {QStringLiteral("bitrate_max_kbps"), packet.bitrate_max_kbps},
                {QStringLiteral("vbv_buffer_kb"), packet.vbv_buffer_kb},
                {QStringLiteral("video_fps"), packet.video_fps},
                {QStringLiteral("depth_fps"), packet.depth_fps},
                {QStringLiteral("profile_mode"), packet.profile_mode},
                {QStringLiteral("enable_emitter"), packet.enable_emitter},
                {QStringLiteral("camera_id"), packet.camera_id},
                {QStringLiteral("is_default"), packet.is_default},
                {QStringLiteral("camera_status"), packet.camera_status},
                {QStringLiteral("error_code"), packet.error_code},
                {QStringLiteral("usb_speed_mode"), packet.usb_speed_mode},
                {QStringLiteral("rtsp_port"), packet.rtsp_port},
                {QStringLiteral("camera_name"), fixedMavlinkString(packet.camera_name)},
                {QStringLiteral("connection_port"), fixedMavlinkString(packet.connection_port)},
                {QStringLiteral("camera_type"), fixedMavlinkString(packet.camera_type)},
                {QStringLiteral("serial_number"), fixedMavlinkString(packet.serial_number)},
                {QStringLiteral("codec"), fixedMavlinkString(packet.codec)},
                {QStringLiteral("encoder_mode"), fixedMavlinkString(packet.encoder_mode)},
                {QStringLiteral("rtsp_url_qgc"), fixedMavlinkString(packet.rtsp_url_qgc)},
                {QStringLiteral("rtsp_url_controller"), fixedMavlinkString(packet.rtsp_url_controller)},
                {QStringLiteral("rtsp_url_laptop"), fixedMavlinkString(packet.rtsp_url_laptop)},
            };
            emit cameraChanged();
            _markReceived(_cameraState, &CompanionController::cameraStatusChanged);
            break;
        }
        case MAVLINK_MSG_ID_CC_TELEMETRY_NETWORK: {
            mavlink_cc_telemetry_network_t packet{};
            mavlink_msg_cc_telemetry_network_decode(&message, &packet);
            _network = {
                {QStringLiteral("ap_channel"), packet.ap_channel},
                {QStringLiteral("ap_ieee80211n"), packet.ap_ieee80211n},
                {QStringLiteral("ap_wmm_enabled"), packet.ap_wmm_enabled},
                {QStringLiteral("ap_wpa"), packet.ap_wpa},
                {QStringLiteral("ap_client_count"), packet.ap_client_count},
                {QStringLiteral("wlan0_dhcp"), packet.wlan0_dhcp},
                {QStringLiteral("dnsmasq_status"), packet.dnsmasq_status},
                {QStringLiteral("ap_status"), packet.ap_status},
                {QStringLiteral("wlan0_status"), packet.wlan0_status},
                {QStringLiteral("eth0_status"), packet.eth0_status},
                {QStringLiteral("eth0_is_static"), packet.eth0_is_static},
                {QStringLiteral("wlan0_rssi"), packet.wlan0_rssi},
                {QStringLiteral("uap0_rx_kb"), qulonglong{packet.uap0_rx_kb}},
                {QStringLiteral("uap0_tx_kb"), qulonglong{packet.uap0_tx_kb}},
                {QStringLiteral("wlan0_rx_kb"), qulonglong{packet.wlan0_rx_kb}},
                {QStringLiteral("wlan0_tx_kb"), qulonglong{packet.wlan0_tx_kb}},
                {QStringLiteral("eth0_rx_kb"), qulonglong{packet.eth0_rx_kb}},
                {QStringLiteral("eth0_tx_kb"), qulonglong{packet.eth0_tx_kb}},
                {QStringLiteral("eth0_ip"), fixedMavlinkString(packet.eth0_ip)},
                {QStringLiteral("eth0_netmask"), fixedMavlinkString(packet.eth0_netmask)},
                {QStringLiteral("wlan0_ip"), fixedMavlinkString(packet.wlan0_ip)},
                {QStringLiteral("wlan0_netmask"), fixedMavlinkString(packet.wlan0_netmask)},
                {QStringLiteral("wlan0_ssid"), fixedMavlinkString(packet.wlan0_ssid)},
                {QStringLiteral("ap_ip"), fixedMavlinkString(packet.ap_ip)},
                {QStringLiteral("ap_netmask"), fixedMavlinkString(packet.ap_netmask)},
                {QStringLiteral("ap_ssid"), fixedMavlinkString(packet.ap_ssid)},
                {QStringLiteral("ap_wpa_passphrase"), fixedMavlinkString(packet.ap_wpa_passphrase)},
                {QStringLiteral("ap_key_mgmt"), fixedMavlinkString(packet.ap_key_mgmt)},
                {QStringLiteral("ap_hw_mode"), fixedMavlinkString(packet.ap_hw_mode)},
            };
            emit networkChanged();
            _markReceived(_networkState, &CompanionController::networkStatusChanged);
            break;
        }
        case MAVLINK_MSG_ID_CC_TELEMETRY_VISION: {
            mavlink_cc_telemetry_vision_t packet{};
            mavlink_msg_cc_telemetry_vision_decode(&message, &packet);
            _vision = {
                {QStringLiteral("confidence_thresh"), packet.confidence_thresh},
                {QStringLiteral("inference_fps"), packet.inference_fps},
                {QStringLiteral("input_width"), packet.input_width},
                {QStringLiteral("input_height"), packet.input_height},
                {QStringLiteral("video_fps"), packet.video_fps},
                {QStringLiteral("detections_count"), packet.detections_count},
                {QStringLiteral("status_flags"), packet.status_flags},
                {QStringLiteral("model_name"), fixedMavlinkString(packet.model_name)},
                {QStringLiteral("input_source"), fixedMavlinkString(packet.input_source)},
            };
            emit visionChanged();
            _markReceived(_visionState, &CompanionController::visionStatusChanged);
            break;
        }
        case MAVLINK_MSG_ID_CC_TELEMETRY_SYSTEM: {
            mavlink_cc_telemetry_system_t packet{};
            mavlink_msg_cc_telemetry_system_decode(&message, &packet);
            _system = {{QStringLiteral("system_uptime_s"), qulonglong{packet.system_uptime_s}},
                       {QStringLiteral("cpu_usage"), packet.cpu_usage},
                       {QStringLiteral("ram_usage"), packet.ram_usage},
                       {QStringLiteral("disk_usage"), packet.disk_usage},
                       {QStringLiteral("cpu_temp"), packet.cpu_temp},
                       {QStringLiteral("system_status"), packet.system_status}};
            emit systemChanged();
            _markReceived(_systemState, &CompanionController::systemStatusChanged);
            break;
        }
        default:
            break;
    }
}

void CompanionController::_markReceived(MessageState& state, void (CompanionController::*statusSignal)())
{
    const bool statusChanged = !state.received || state.stale;
    state.received = true;
    state.stale = false;
    state.lastReceivedMs = _clock.elapsed();
    if (statusChanged) {
        (this->*statusSignal)();
    }
}

void CompanionController::_updateStale(MessageState& state, void (CompanionController::*statusSignal)())
{
    const bool stale = state.received && ((_clock.elapsed() - state.lastReceivedMs) >= kStaleTimeoutMs);
    if (state.stale != stale) {
        state.stale = stale;
        (this->*statusSignal)();
    }
}

void CompanionController::_updateStaleStates()
{
    _updateStale(_linksState, &CompanionController::linksStatusChanged);
    _updateStale(_cameraState, &CompanionController::cameraStatusChanged);
    _updateStale(_networkState, &CompanionController::networkStatusChanged);
    _updateStale(_visionState, &CompanionController::visionStatusChanged);
    _updateStale(_systemState, &CompanionController::systemStatusChanged);
    _updateStale(_missionState, &CompanionController::missionStatusChanged);
}

void CompanionController::_resetTelemetry()
{
    _links = emptyLinks();
    _camera = emptyCamera();
    _network = emptyNetwork();
    _vision = emptyVision();
    _system.clear();
    _linksPacket = {};
    _linksState = {};
    _cameraState = {};
    _networkState = {};
    _visionState = {};
    _systemState = {};
    _missionState = {};
    _lastTriggerId = 0;
    _lastTriggerTimeBootMs = 0;
    _sourceSystemId = -1;
    _sourceComponentId = -1;

    emit linksChanged();
    emit cameraChanged();
    emit networkChanged();
    emit visionChanged();
    emit systemChanged();
    emit missionChanged();
    emit linksStatusChanged();
    emit cameraStatusChanged();
    emit networkStatusChanged();
    emit visionStatusChanged();
    emit systemStatusChanged();
    emit missionStatusChanged();
    emit sourceChanged();
}

#ifdef QGC_UNITTEST_BUILD
void CompanionController::processMessageForTest(const mavlink_message_t& message)
{
    if (message.msgid == MAVLINK_MSG_ID_THACO_EXTERNAL_XYZ_TRIGGER) {
        mavlink_thaco_external_xyz_trigger_t trigger{};
        mavlink_msg_thaco_external_xyz_trigger_decode(&message, &trigger);
        _lastTriggerId = trigger.trigger_id;
        _lastTriggerTimeBootMs = trigger.time_boot_ms;
        emit missionChanged();
        _markReceived(_missionState, &CompanionController::missionStatusChanged);
        return;
    }
    if (isCcTelemetryMessage(message) && _acceptSource(message, false)) {
        _processMessage(message);
    }
}

void CompanionController::forceStaleForTest()
{
    const qint64 staleTimestamp = _clock.elapsed() - kStaleTimeoutMs - 1;
    _linksState.lastReceivedMs = staleTimestamp;
    _cameraState.lastReceivedMs = staleTimestamp;
    _networkState.lastReceivedMs = staleTimestamp;
    _visionState.lastReceivedMs = staleTimestamp;
    _systemState.lastReceivedMs = staleTimestamp;
    _updateStaleStates();
}

void CompanionController::resetForTest()
{
    _resetTelemetry();
}
#endif
