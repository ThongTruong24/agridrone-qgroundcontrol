#include "CcTelemetryController.h"

#include <algorithm>
#include <cstddef>

#include "MultiVehicleManager.h"
#include "Vehicle.h"

namespace {
template <std::size_t Size>
QString fixedMavlinkString(const char (&text)[Size])
{
    const auto end = std::find(text, text + Size, '\0');
    return QString::fromUtf8(text, static_cast<qsizetype>(end - text));
}

QVariantMap emptyLinks()
{
    return {
        {QStringLiteral("fc_baudrate"), qulonglong{0}}, {QStringLiteral("siyi_baudrate"), qulonglong{0}},
        {QStringLiteral("fc_bytes_rx"), qulonglong{0}}, {QStringLiteral("fc_bytes_tx"), qulonglong{0}},
        {QStringLiteral("fc_bitrate_kbps"), 0.0},       {QStringLiteral("link_status_flags"), 0},
        {QStringLiteral("fc_port"), QString()},         {QStringLiteral("siyi_port"), QString()},
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
        {QStringLiteral("rtsp_url"), QString()},
    };
}

QVariantMap emptyNetwork()
{
    return {
        {QStringLiteral("ap_channel"), 0},          {QStringLiteral("ap_ieee80211n"), 0},
        {QStringLiteral("ap_wmm_enabled"), 0},      {QStringLiteral("ap_wpa"), 0},
        {QStringLiteral("ap_client_count"), 0},     {QStringLiteral("wlan0_dhcp"), 0},
        {QStringLiteral("dnsmasq_status"), 0},      {QStringLiteral("eth0_ip"), QString()},
        {QStringLiteral("wlan0_ip"), QString()},    {QStringLiteral("ap_ip"), QString()},
        {QStringLiteral("ap_ssid"), QString()},     {QStringLiteral("ap_wpa_passphrase"), QString()},
        {QStringLiteral("ap_key_mgmt"), QString()}, {QStringLiteral("ap_hw_mode"), QString()},
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
            return true;
        default:
            return false;
    }
}
}  // namespace

CcTelemetryController::CcTelemetryController(QObject* parent)
    : QObject(parent), _links(emptyLinks()), _camera(emptyCamera()), _network(emptyNetwork()), _vision(emptyVision())
{
    _clock.start();
    _staleTimer.setInterval(kStaleCheckIntervalMs);
    _staleTimer.setSingleShot(false);
    (void) connect(&_staleTimer, &QTimer::timeout, this, &CcTelemetryController::_updateStaleStates);
    _staleTimer.start();

    MultiVehicleManager* const multiVehicleManager = MultiVehicleManager::instance();
    (void) connect(multiVehicleManager, &MultiVehicleManager::activeVehicleChanged, this,
                   &CcTelemetryController::_setActiveVehicle);
    _setActiveVehicle(multiVehicleManager->activeVehicle());
}

void CcTelemetryController::_setActiveVehicle(Vehicle* vehicle)
{
    if (_activeVehicle == vehicle) {
        return;
    }

    const bool wasAvailable = vehicleAvailable();
    QObject::disconnect(_vehicleMessageConnection);
    QObject::disconnect(_vehicleDestroyedConnection);
    _activeVehicle = vehicle;
    _resetTelemetry();

    if (_activeVehicle) {
        _vehicleMessageConnection = connect(_activeVehicle, &Vehicle::mavlinkMessageReceived, this,
                                            &CcTelemetryController::_mavlinkMessageReceived, Qt::UniqueConnection);
        _vehicleDestroyedConnection = connect(_activeVehicle, &QObject::destroyed, this, [this]() {
            _activeVehicle = nullptr;
            _resetTelemetry();
            emit vehicleAvailableChanged();
        });
    }

    if (wasAvailable != vehicleAvailable()) {
        emit vehicleAvailableChanged();
    }
}

void CcTelemetryController::_mavlinkMessageReceived(const mavlink_message_t& message)
{
    if (!isCcTelemetryMessage(message) || !_acceptSource(message, true)) {
        return;
    }
    _processMessage(message);
}

bool CcTelemetryController::_acceptSource(const mavlink_message_t& message, bool requireActiveVehicle)
{
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

void CcTelemetryController::_processMessage(const mavlink_message_t& message)
{
    switch (message.msgid) {
        case MAVLINK_MSG_ID_CC_TELEMETRY_LINKS: {
            mavlink_cc_telemetry_links_t packet{};
            mavlink_msg_cc_telemetry_links_decode(&message, &packet);
            _links = {
                {QStringLiteral("fc_baudrate"), qulonglong{packet.fc_baudrate}},
                {QStringLiteral("siyi_baudrate"), qulonglong{packet.siyi_baudrate}},
                {QStringLiteral("fc_bytes_rx"), qulonglong{packet.fc_bytes_rx}},
                {QStringLiteral("fc_bytes_tx"), qulonglong{packet.fc_bytes_tx}},
                {QStringLiteral("fc_bitrate_kbps"), packet.fc_bitrate_kbps},
                {QStringLiteral("link_status_flags"), packet.link_status_flags},
                {QStringLiteral("fc_port"), fixedMavlinkString(packet.fc_port)},
                {QStringLiteral("siyi_port"), fixedMavlinkString(packet.siyi_port)},
            };
            emit linksChanged();
            _markReceived(_linksState, &CcTelemetryController::linksStatusChanged);
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
                {QStringLiteral("camera_type"), fixedMavlinkString(packet.camera_type)},
                {QStringLiteral("serial_number"), fixedMavlinkString(packet.serial_number)},
                {QStringLiteral("codec"), fixedMavlinkString(packet.codec)},
                {QStringLiteral("encoder_mode"), fixedMavlinkString(packet.encoder_mode)},
                {QStringLiteral("rtsp_url"), fixedMavlinkString(packet.rtsp_url)},
            };
            emit cameraChanged();
            _markReceived(_cameraState, &CcTelemetryController::cameraStatusChanged);
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
                {QStringLiteral("eth0_ip"), fixedMavlinkString(packet.eth0_ip)},
                {QStringLiteral("wlan0_ip"), fixedMavlinkString(packet.wlan0_ip)},
                {QStringLiteral("ap_ip"), fixedMavlinkString(packet.ap_ip)},
                {QStringLiteral("ap_ssid"), fixedMavlinkString(packet.ap_ssid)},
                {QStringLiteral("ap_wpa_passphrase"), fixedMavlinkString(packet.ap_wpa_passphrase)},
                {QStringLiteral("ap_key_mgmt"), fixedMavlinkString(packet.ap_key_mgmt)},
                {QStringLiteral("ap_hw_mode"), fixedMavlinkString(packet.ap_hw_mode)},
            };
            emit networkChanged();
            _markReceived(_networkState, &CcTelemetryController::networkStatusChanged);
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
            _markReceived(_visionState, &CcTelemetryController::visionStatusChanged);
            break;
        }
        default:
            break;
    }
}

void CcTelemetryController::_markReceived(MessageState& state, void (CcTelemetryController::*statusSignal)())
{
    const bool statusChanged = !state.received || state.stale;
    state.received = true;
    state.stale = false;
    state.lastReceivedMs = _clock.elapsed();
    if (statusChanged) {
        (this->*statusSignal)();
    }
}

void CcTelemetryController::_updateStale(MessageState& state, void (CcTelemetryController::*statusSignal)())
{
    const bool stale = state.received && ((_clock.elapsed() - state.lastReceivedMs) >= kStaleTimeoutMs);
    if (state.stale != stale) {
        state.stale = stale;
        (this->*statusSignal)();
    }
}

void CcTelemetryController::_updateStaleStates()
{
    _updateStale(_linksState, &CcTelemetryController::linksStatusChanged);
    _updateStale(_cameraState, &CcTelemetryController::cameraStatusChanged);
    _updateStale(_networkState, &CcTelemetryController::networkStatusChanged);
    _updateStale(_visionState, &CcTelemetryController::visionStatusChanged);
}

void CcTelemetryController::_resetTelemetry()
{
    _links = emptyLinks();
    _camera = emptyCamera();
    _network = emptyNetwork();
    _vision = emptyVision();
    _linksState = {};
    _cameraState = {};
    _networkState = {};
    _visionState = {};
    _sourceSystemId = -1;
    _sourceComponentId = -1;

    emit linksChanged();
    emit cameraChanged();
    emit networkChanged();
    emit visionChanged();
    emit linksStatusChanged();
    emit cameraStatusChanged();
    emit networkStatusChanged();
    emit visionStatusChanged();
    emit sourceChanged();
}

#ifdef QGC_UNITTEST_BUILD
void CcTelemetryController::processMessageForTest(const mavlink_message_t& message)
{
    if (isCcTelemetryMessage(message) && _acceptSource(message, false)) {
        _processMessage(message);
    }
}

void CcTelemetryController::forceStaleForTest()
{
    const qint64 staleTimestamp = _clock.elapsed() - kStaleTimeoutMs - 1;
    _linksState.lastReceivedMs = staleTimestamp;
    _cameraState.lastReceivedMs = staleTimestamp;
    _networkState.lastReceivedMs = staleTimestamp;
    _visionState.lastReceivedMs = staleTimestamp;
    _updateStaleStates();
}

void CcTelemetryController::resetForTest()
{
    _resetTelemetry();
}
#endif
