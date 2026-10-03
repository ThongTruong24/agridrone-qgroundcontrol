#include <QTimer>
#include "CompanionController.h"
#include "MultiVehicleManager.h"
#include "MAVLinkProtocol.h"
#include "VehicleLinkManager.h"
#include "LinkManager.h"

#include <QtCore/QDebug>
#include <QtCore/QByteArray>

static constexpr uint8_t kCompanionCompId = 191; // MAV_COMP_ID_ONBOARD_COMPUTER

CompanionController::CompanionController(QObject* parent)
    : QObject(parent)
{
    // SOLID: Decoupled services
    _logService = new CompanionLogService(this);
    _linksService = new CompanionLinksService(this);
    _paramService = new CompanionParamService(this);
    _dispatcher = new CompanionMavlinkDispatcher(this);

    // Register handlers to dispatcher (SOLID: Open/Closed & Dependency Inversion)
    _dispatcher->registerHandler(_linksService);
    _dispatcher->registerHandler(_paramService);

    // Connect service signals to facade
    connect(_linksService, &CompanionLinksService::linksChanged, this, [this]() {
        emit linksChanged();
        emit vehicleConnectedChanged();
    });
    connect(_linksService, &CompanionLinksService::availablePortsChanged, this, &CompanionController::availablePortsChanged);
    connect(_paramService, &CompanionParamService::paramListChanged, this, &CompanionController::ccParametersChanged);
    connect(_paramService, &CompanionParamService::isLoadingChanged, this, &CompanionController::ccParametersLoadingChanged);
    connect(_paramService, &CompanionParamService::modifiedCountChanged, this, &CompanionController::ccModifiedParamCountChanged);
    connect(_paramService, &CompanionParamService::groupsChanged, this, &CompanionController::ccParameterGroupsChanged);
    connect(_paramService, &CompanionParamService::parameterSaved, this, [this](const QString& name, bool success, const QString& msg) {
        Q_UNUSED(name);
        _showToast(msg, !success);
    });
    connect(_paramService, &CompanionParamService::logMessage, _logService, &CompanionLogService::logMavlink);
    connect(_linksService, &CompanionLinksService::logMessage, _logService, &CompanionLogService::logMavlink);
    connect(_logService, &CompanionLogService::logEntriesChanged, this, &CompanionController::logEntriesChanged);
    connect(_logService, &CompanionLogService::logMessageAdded, this, &CompanionController::mavlinkLogMessage);

    MultiVehicleManager* const multiVehicleManager = MultiVehicleManager::instance();
    connect(multiVehicleManager, &MultiVehicleManager::activeVehicleChanged,
            this, &CompanionController::_setActiveVehicle);
    _setActiveVehicle(multiVehicleManager->activeVehicle());

    // Ingest all MAVLink messages directly via MAVLinkProtocol (same as MAVLinkInspector)
    connect(MAVLinkProtocol::instance(), &MAVLinkProtocol::messageReceived,
            this, [this](LinkInterface* link, const mavlink_message_t& message) {
                Q_UNUSED(link);
                _onMavlinkMessageReceived(message);
            });

    _telemetryWatchdog = new QTimer(this);
    _telemetryWatchdog->setInterval(3500); // 3.5s timeout for 1Hz telemetry
    connect(_telemetryWatchdog, &QTimer::timeout, this, [this]() {
        if (_hasSystemTelemetry || (_linksService && _linksService->hasLinksTelemetry())) {
            _clearTelemetryState();
            _showToast(QStringLiteral("Companion Telemetry stream timed out (OFFLINE)"), true);
        }
    });
}

bool CompanionController::vehicleConnected() const
{
    return (_activeVehicle != nullptr) || _hasSystemTelemetry || (_linksService && _linksService->hasLinksTelemetry());
}

void CompanionController::_clearTelemetryState()
{
    if (_telemetryWatchdog) {
        _telemetryWatchdog->stop();
    }

    if (_dispatcher) {
        _dispatcher->resetAllHandlers();
    }

    _hasCameraTelemetry = false;
    _hasVisionTelemetry = false;
    _hasNetworkTelemetry = false;
    _hasSystemTelemetry = false;
    _hasMissionTelemetry = false;

    _camStatus = 0;
    _camName.clear();
    _cameraType.clear();
    _connectionPort.clear();
    _serialNumber.clear();
    _videoWidth = 0;
    _videoHeight = 0;
    _videoFps = 0;
    _depthWidth = 0;
    _depthHeight = 0;
    _depthFps = 0;
    _bitrateKbps = 0;
    _bitrateMaxKbps = 0;
    _vbvBufferKb = 0;
    _rotation = 0;
    _rtspPort = 8554;
    _rtspUrlQgc.clear();
    _rtspUrlController.clear();
    _rtspUrlLaptop.clear();
    _codec.clear();
    _encoderMode.clear();
    _isDefaultCam = false;
    _profileMode = 0;
    _enableEmitter = 0;
    _cameraStatus = 0;
    _errorCode = 0;
    _usbSpeedMode = 0;

    _confidenceThresh = 0.0f;
    _inferenceFps = 0.0f;
    _visionInputWidth = 0;
    _visionInputHeight = 0;
    _visionVideoFps = 0;
    _detectionsCount = 0;
    _visionStatusFlags = 0;
    _modelName.clear();
    _inputSource.clear();

    _eth0Ip.clear();
    _eth0Netmask.clear();
    _eth0Status = 0;
    _eth0IsStatic = 0;
    _eth0RxKb = 0;
    _eth0TxKb = 0;
    _wlan0Ip.clear();
    _wlan0Netmask.clear();
    _wlan0Status = 0;
    _wlan0Ssid.clear();
    _wlan0Rssi = 0;
    _wlan0Dhcp = 0;
    _wlan0RxKb = 0;
    _wlan0TxKb = 0;
    _apIp.clear();
    _apNetmask.clear();
    _apSsid.clear();
    _apChannel = 0;
    _apClientCount = 0;
    _apStatus = 0;
    _apHwMode.clear();
    _uap0RxKb = 0;
    _uap0TxKb = 0;
    _dnsmasqStatus = 0;

    _cpuUsage = 0;
    _ramUsage = 0;
    _diskUsage = 0;
    _cpuTemp = 0;
    _uptimeS = 0;

    emit cameraChanged();
    emit visionChanged();
    emit networkChanged();
    emit systemChanged();
    emit missionChanged();
    emit vehicleConnectedChanged();
}

void CompanionController::_setActiveVehicle(Vehicle* vehicle)
{
    if (_activeVehicle == vehicle) return;

    if (_activeVehicle) {
        _clearTelemetryState();
        _sourceSystemId    = -1;
        _sourceComponentId = -1;
        _vehicleEpoch++;
        emit vehicleEpochChanged();
    }

    _activeVehicle = vehicle;
    emit vehicleConnectedChanged();
    emit vehicleAvailableChanged();

    if (_activeVehicle) {
        connect(_activeVehicle, &Vehicle::mavlinkMessageReceived, this,
                &CompanionController::_onMavlinkMessageReceived, Qt::UniqueConnection);
        // connected
    }
}

void CompanionController::_logMavlink(const QString& category, const QString& direction, const QString& message, int severity)
{
    if (_logService) {
        _logService->logMavlink(category, direction, message, severity);
    }
}

QVariantList CompanionController::getLogHistory(const QString& category) const
{
    return _logService ? _logService->getLogHistory(category) : QVariantList();
}

void CompanionController::clearLogHistory(const QString& category)
{
    if (_logService) {
        _logService->clearLogHistory(category);
    }
}

void CompanionController::_showToast(const QString& msg, bool isError)
{
    _lastToastMsg = msg;
    _lastToastIsError = isError;
    emit toastChanged();

    // Auto-clear toast after 4s so it never stays stuck as a permanent status
    QTimer::singleShot(4000, this, [this, msg]() {
        if (_lastToastMsg == msg) {
            _lastToastMsg.clear();
            _lastToastIsError = false;
            emit toastChanged();
        }
    });
}

void CompanionController::_onMavlinkMessageReceived(const mavlink_message_t& message)
{
    if (!_activeVehicle) {
        Vehicle* active = MultiVehicleManager::instance()->activeVehicle();
        if (active) {
            _setActiveVehicle(active);
        }
    }

    // Require active vehicle match if active vehicle is known
    if (_activeVehicle && message.sysid != _activeVehicle->id()) {
        return;
    }

    if (message.msgid == 32000) { // MAVLINK_MSG_ID_THACO_EXTERNAL_XYZ_TRIGGER
        mavlink_thaco_external_xyz_trigger_t trigger{};
        mavlink_msg_thaco_external_xyz_trigger_decode(&message, &trigger);
        _lastTriggerId = trigger.trigger_id;
        _lastTriggerTimeBootMs = trigger.time_boot_ms;
        _hasMissionTelemetry = true;
        emit missionChanged();
        _logMavlink(QStringLiteral("MISSION"), QStringLiteral("RX"),
                    QStringLiteral("MISSION: XYZ trigger %1 at %2 ms").arg(trigger.trigger_id).arg(trigger.time_boot_ms), 6);
        return;
    }

    if (message.msgid == 253) { // MAVLINK_MSG_ID_STATUSTEXT
        if (message.compid != kCompanionCompId) return;
        mavlink_statustext_t st;
        mavlink_msg_statustext_decode(&message, &st);
        QString text = QString::fromUtf8(st.text, qstrnlen(st.text, sizeof(st.text))).trimmed();
        if (!text.isEmpty()) {
            QString category = QStringLiteral("ALL");
            if (text.startsWith(QStringLiteral("SIYI:"))) {
                category = QStringLiteral("SIYI");
            } else if (text.startsWith(QStringLiteral("FC:"))) {
                category = QStringLiteral("FC");
            } else if (text.startsWith(QStringLiteral("NET:"))) {
                category = QStringLiteral("NETWORK");
            } else if (text.startsWith(QStringLiteral("CAM:"))) {
                category = QStringLiteral("CAMERA");
            } else if (text.startsWith(QStringLiteral("SYS:"))) {
                category = QStringLiteral("SYSTEM");
            } else if (text.startsWith(QStringLiteral("VIS:"))) {
                category = QStringLiteral("VISION");
            } else {
                if (_lastAppliedCategory == QStringLiteral("FC")) category = QStringLiteral("FC");
                else if (_lastAppliedCategory == QStringLiteral("SIYI")) category = QStringLiteral("SIYI");
            }
            _logMavlink(category, QStringLiteral("RX"), text, st.severity);
        }
        return;
    }

    if (message.msgid == 77) { // MAVLINK_MSG_ID_COMMAND_ACK
        if (message.compid != kCompanionCompId) return;
        mavlink_command_ack_t ack;
        mavlink_msg_command_ack_decode(&message, &ack);
        if (ack.target_system != 0 && ack.target_system != MAVLinkProtocol::instance()->getSystemId()) return;
        if (ack.target_component != 0 && ack.target_component != MAVLinkProtocol::getComponentId()) return;
        if (_pendingCommand != 0 && ack.command != _pendingCommand) return;

        if (ack.result == MAV_RESULT_IN_PROGRESS) {
            _configStatus = QStringLiteral("Applying");
            emit configStatusChanged();
            return;
        }

        const quint16 completedCommand = _pendingCommand;
        _pendingCommand = 0;
        if (_configTimer) _configTimer->stop();

        emit commandAckReceived(ack.command, ack.result, QString("ACK command %1 result %2").arg(ack.command).arg(ack.result));

        if (ack.result == MAV_RESULT_ACCEPTED) {
            if (completedCommand == 44011) {
                _waitingTelemetry = true;
                _configStatus = QStringLiteral("WaitingTelemetry");
                _configMessage = QStringLiteral("ACK accepted; waiting for UART telemetry");
                emit configStatusChanged();
                if (!_confirmTimer) {
                    _confirmTimer = new QTimer(this);
                    _confirmTimer->setSingleShot(true);
                    connect(_confirmTimer, &QTimer::timeout, this, &CompanionController::_confirmationTimedOut);
                }
                _confirmTimer->start(6000);
            } else {
                _configStatus = QStringLiteral("Success");
                _configMessage = QStringLiteral("Companion saved UART defaults");
                emit configStatusChanged();
            }
            QString logTxt = QString("[RX ACK] Command %1 accepted").arg(completedCommand);
            _logMavlink(QStringLiteral("UART"), QStringLiteral("RX"), logTxt, 6);
        } else {
            _configStatus = QStringLiteral("Failed");
            _configMessage = QStringLiteral("Companion rejected command (%1)").arg(ack.result);
            emit configStatusChanged();
            QString logTxt = QString("[RX ACK] Command %1 denied (%2)").arg(completedCommand).arg(ack.result);
            _logMavlink(QStringLiteral("UART"), QStringLiteral("RX"), logTxt, 3);
        }
        return;
    }

    // Telemetry messages (42010..42014): must come from Companion Computer (compid 191)
    if (message.compid != kCompanionCompId) {
        return;
    }

    if (_sourceComponentId < 0) {
        _sourceSystemId    = message.sysid;
        _sourceComponentId = message.compid;
    } else if (message.sysid != _sourceSystemId || message.compid != _sourceComponentId) {
        return;
    }

    if (_telemetryWatchdog) {
        _telemetryWatchdog->start();
    }

    // Telemetry stream active -> dismiss stale offline/timeout toast immediately
    if (_lastToastMsg.contains(QStringLiteral("timed out")) || _lastToastMsg.contains(QStringLiteral("OFFLINE"))) {
        _lastToastMsg.clear();
        _lastToastIsError = false;
        emit toastChanged();
    }

    // Dispatch to registered handlers first (Links, etc.)
    if (_dispatcher && _dispatcher->dispatchMessage(message)) {
        if (message.msgid == 42010) {
            _checkTelemetryConfirmation();
        }
        return;
    }

    switch (message.msgid) {
        case 42011: { // CC_TELEMETRY_CAMERA
        mavlink_cc_telemetry_camera_t cam;
        mavlink_msg_cc_telemetry_camera_decode(&message, &cam);

        _hasCameraTelemetry = true;
        _camStatus = cam.camera_status;
        _camName = QString::fromUtf8(cam.camera_name, qstrnlen(cam.camera_name, sizeof(cam.camera_name)));
        _cameraType = QString::fromUtf8(cam.camera_type, qstrnlen(cam.camera_type, sizeof(cam.camera_type)));
        _connectionPort = QString::fromUtf8(cam.connection_port, qstrnlen(cam.connection_port, sizeof(cam.connection_port)));
        _serialNumber = QString::fromUtf8(cam.serial_number, qstrnlen(cam.serial_number, sizeof(cam.serial_number)));
        _videoWidth = cam.video_width;
        _videoHeight = cam.video_height;
        _videoFps = cam.video_fps;
        _depthWidth = cam.depth_width;
        _depthHeight = cam.depth_height;
        _depthFps = cam.depth_fps;
        _bitrateKbps = cam.bitrate_kbps;
        _bitrateMaxKbps = cam.bitrate_max_kbps;
        _vbvBufferKb = cam.vbv_buffer_kb;
        _rotation = cam.rotation;
        _rtspPort = cam.rtsp_port;
        _rtspUrlQgc = QString::fromUtf8(cam.rtsp_url_qgc, qstrnlen(cam.rtsp_url_qgc, sizeof(cam.rtsp_url_qgc)));
        _rtspUrlController = QString::fromUtf8(cam.rtsp_url_controller, qstrnlen(cam.rtsp_url_controller, sizeof(cam.rtsp_url_controller)));
        _rtspUrlLaptop = QString::fromUtf8(cam.rtsp_url_laptop, qstrnlen(cam.rtsp_url_laptop, sizeof(cam.rtsp_url_laptop)));
        _codec = QString::fromUtf8(cam.codec, qstrnlen(cam.codec, sizeof(cam.codec)));
        _encoderMode = QString::fromUtf8(cam.encoder_mode, qstrnlen(cam.encoder_mode, sizeof(cam.encoder_mode)));
        _isDefaultCam = cam.is_default != 0;
        _profileMode = cam.profile_mode;
        _enableEmitter = cam.enable_emitter;
        _cameraStatus = cam.camera_status;
        _errorCode = cam.error_code;
        _usbSpeedMode = cam.usb_speed_mode;

        emit cameraChanged();
        break;
    }

    case 42012: { // CC_TELEMETRY_NETWORK
        mavlink_cc_telemetry_network_t net;
        mavlink_msg_cc_telemetry_network_decode(&message, &net);

        _hasNetworkTelemetry = true;
        _eth0Ip = QString::fromUtf8(net.eth0_ip, qstrnlen(net.eth0_ip, sizeof(net.eth0_ip)));
        _eth0Netmask = QString::fromUtf8(net.eth0_netmask, qstrnlen(net.eth0_netmask, sizeof(net.eth0_netmask)));
        _eth0Status = net.eth0_status;
        _eth0IsStatic = net.eth0_is_static;
        _eth0RxKb = net.eth0_rx_kb;
        _eth0TxKb = net.eth0_tx_kb;
        _wlan0Ip = QString::fromUtf8(net.wlan0_ip, qstrnlen(net.wlan0_ip, sizeof(net.wlan0_ip)));
        _wlan0Netmask = QString::fromUtf8(net.wlan0_netmask, qstrnlen(net.wlan0_netmask, sizeof(net.wlan0_netmask)));
        _wlan0Status = net.wlan0_status;
        _wlan0Ssid = QString::fromUtf8(net.wlan0_ssid, qstrnlen(net.wlan0_ssid, sizeof(net.wlan0_ssid)));
        _wlan0Rssi = net.wlan0_rssi;
        _wlan0Dhcp = net.wlan0_dhcp;
        _wlan0RxKb = net.wlan0_rx_kb;
        _wlan0TxKb = net.wlan0_tx_kb;
        _apIp = QString::fromUtf8(net.ap_ip, qstrnlen(net.ap_ip, sizeof(net.ap_ip)));
        _apNetmask = QString::fromUtf8(net.ap_netmask, qstrnlen(net.ap_netmask, sizeof(net.ap_netmask)));
        _apSsid = QString::fromUtf8(net.ap_ssid, qstrnlen(net.ap_ssid, sizeof(net.ap_ssid)));
        _apChannel = net.ap_channel;
        _apClientCount = net.ap_client_count;
        _apStatus = net.ap_status;
        _apHwMode = QString::fromUtf8(net.ap_hw_mode, qstrnlen(net.ap_hw_mode, sizeof(net.ap_hw_mode)));
        _uap0RxKb = net.uap0_rx_kb;
        _uap0TxKb = net.uap0_tx_kb;
        _dnsmasqStatus = net.dnsmasq_status;
        _apIeee80211n    = net.ap_ieee80211n;
        _apWmmEnabled    = net.ap_wmm_enabled;
        _apWpa           = net.ap_wpa;
        _apWpaPassphrase = QString::fromUtf8(net.ap_wpa_passphrase,
                               qstrnlen(net.ap_wpa_passphrase, sizeof(net.ap_wpa_passphrase)));
        _apKeyMgmt       = QString::fromUtf8(net.ap_key_mgmt,
                               qstrnlen(net.ap_key_mgmt, sizeof(net.ap_key_mgmt)));

        emit networkChanged();
        break;
    }

    case 42013: { // CC_TELEMETRY_VISION
        mavlink_cc_telemetry_vision_t vis;
        mavlink_msg_cc_telemetry_vision_decode(&message, &vis);

        _hasVisionTelemetry = true;
        _confidenceThresh = vis.confidence_thresh;
        _inferenceFps = vis.inference_fps;
        _visionInputWidth = vis.input_width;
        _visionInputHeight = vis.input_height;
        _visionVideoFps = vis.video_fps;
        _detectionsCount = vis.detections_count;
        _visionStatusFlags = vis.status_flags;
        _modelName = QString::fromUtf8(vis.model_name, qstrnlen(vis.model_name, sizeof(vis.model_name)));
        _inputSource = QString::fromUtf8(vis.input_source, qstrnlen(vis.input_source, sizeof(vis.input_source)));

        emit visionChanged();
        break;
    }

    case 42014: { // CC_TELEMETRY_SYSTEM
        mavlink_cc_telemetry_system_t sys;
        mavlink_msg_cc_telemetry_system_decode(&message, &sys);

        const bool prevConn = vehicleConnected();
        _hasSystemTelemetry = true;
        _cpuUsage = sys.cpu_usage;
        _ramUsage = sys.ram_usage;
        _diskUsage = sys.disk_usage;
        _cpuTemp = sys.cpu_temp;
        _uptimeS = sys.system_uptime_s;

        emit systemChanged();
        if (prevConn != vehicleConnected()) {
            emit vehicleConnectedChanged();
        }
        break;
    }

    case 32000: { // THACO_EXTERNAL_XYZ_TRIGGER
        mavlink_thaco_external_xyz_trigger_t trig;
        mavlink_msg_thaco_external_xyz_trigger_decode(&message, &trig);

        _hasMissionTelemetry = true;
        _lastTriggerId = trig.trigger_id;
        _lastTriggerTimeBootMs = trig.time_boot_ms;

        emit missionChanged();
        break;
    }

    default:
        break;
    }
}

void CompanionController::sendCameraConfig(int width, int height, int fps, int bitrateKbps, int rotation, const QString& codec)
{
    _videoWidth = width;
    _videoHeight = height;
    _videoFps = fps;
    _bitrateKbps = bitrateKbps;
    _rotation = rotation;
    _codec = codec;
    _configStatus = "STAGED";
    emit configStatusChanged();
    _showToast(QString("Staged Camera draft: %1x%2 @ %3fps (%4kbps)").arg(width).arg(height).arg(fps).arg(bitrateKbps), false);
}

void CompanionController::sendVisionConfig(float confidenceThresh, const QString& modelName, const QString& inputSource)
{
    _confidenceThresh = confidenceThresh;
    _modelName = modelName;
    _inputSource = inputSource;
    _configStatus = "STAGED";
    emit configStatusChanged();
    _showToast(QString("Staged Vision draft: Model %1 (Thresh %2)").arg(modelName).arg(confidenceThresh, 0, 'f', 2), false);
}

void CompanionController::sendNetworkConfig(int apChannel, const QString& apSsid, const QString& apPsk, const QString& eth0Ip)
{
    Q_UNUSED(apPsk);
    _apChannel = apChannel;
    _apSsid = apSsid;
    _eth0Ip = eth0Ip;
    _configStatus = "STAGED";
    emit configStatusChanged();
    _showToast(QString("Staged Network draft: SSID %1 (Channel %2)").arg(apSsid).arg(apChannel), false);
}

void CompanionController::sendLinksConfig(int fcBaud, int siyiBaud, const QString& fcPort, const QString& siyiPort)
{
    if (!_activeVehicle || !_linksService) return;
    _linksService->sendLinksConfig(_activeVehicle, fcBaud, siyiBaud, fcPort, siyiPort);
    _configStatus = "STAGED";
    emit configStatusChanged();
}

void CompanionController::applyFcLink(const QString& fcPort, int fcBaud)
{
    if (!_linksService) return;
    QString port = fcPort.trimmed();
    if (port.isEmpty()) {
        port = _linksService->fcPort().trimmed();
    }
    int baud = fcBaud > 0 ? fcBaud : _linksService->fcBaud();

    if (port.isEmpty()) {
        _showToast(QStringLiteral("Vui lòng chọn cổng cho Cube FC!"), true);
        return;
    }
    if (baud <= 0) {
        _showToast(QStringLiteral("Vui lòng chọn baudrate cho Cube FC!"), true);
        return;
    }

    QString siyiPort = _linksService->siyiPort().trimmed();
    int siyiBaud = _linksService->siyiBaud();
    if (siyiPort.isEmpty()) {
        siyiPort = QStringLiteral("/dev/ttyAMA0");
    }
    if (siyiBaud <= 0) {
        siyiBaud = 57600;
    }

    // Guard against port conflict
    if (!port.isEmpty() && port == siyiPort) {
        _showToast(QStringLiteral("Lỗi: Cổng %1 đang được sử dụng bởi SIYI! Vui lòng chọn cổng khác.").arg(port), true);
        _logMavlink(QStringLiteral("FC"), QStringLiteral("ERR"), QStringLiteral("Xung đột cổng: Cube FC và SIYI không thể dùng chung cổng %1").arg(port), 3);
        return;
    }

    _logMavlink(QStringLiteral("FC"), QStringLiteral("TX"), QString("[TX CMD] Apply FC Link -> %1@%2 bps (SIYI preserved: %3@%4 bps)").arg(port).arg(baud).arg(siyiPort).arg(siyiBaud), 5);
    _pendingFcPort    = port;
    _pendingFcBaud    = baud;
    _pendingSiyiPort  = siyiPort;
    _pendingSiyiBaud  = siyiBaud;
    _pendingCommand   = 44011;
    _waitingTelemetry = false;
    _configStatus     = QStringLiteral("Applying");
    emit configStatusChanged();
    if (!_configTimer) {
        _configTimer = new QTimer(this);
        _configTimer->setSingleShot(true);
        connect(_configTimer, &QTimer::timeout, this, &CompanionController::_configTimedOut);
    }
    _configTimer->start(5000);
    sendLinksConfig(baud, siyiBaud, port, siyiPort);
    applyConfig(1, true, QStringLiteral("FC"));
}

void CompanionController::applySiyiLink(const QString& siyiPort, int siyiBaud)
{
    if (!_linksService) return;
    QString port = siyiPort.trimmed();
    if (port.isEmpty()) {
        port = _linksService->siyiPort().trimmed();
    }
    int baud = siyiBaud > 0 ? siyiBaud : _linksService->siyiBaud();

    if (port.isEmpty()) {
        _showToast(QStringLiteral("Vui lòng chọn cổng cho SIYI Link!"), true);
        return;
    }
    if (baud <= 0) {
        _showToast(QStringLiteral("Vui lòng chọn baudrate cho SIYI Link!"), true);
        return;
    }

    QString fcPort = _linksService->fcPort().trimmed();
    int fcBaud = _linksService->fcBaud();
    if (fcPort.isEmpty()) {
        fcPort = QStringLiteral("/dev/ttyAMA4");
    }
    if (fcBaud <= 0) {
        fcBaud = 921600;
    }

    // Guard against port conflict
    if (!port.isEmpty() && port == fcPort) {
        _showToast(QStringLiteral("Lỗi: Cổng %1 đang được sử dụng bởi Cube FC! Vui lòng chọn cổng khác.").arg(port), true);
        _logMavlink(QStringLiteral("SIYI"), QStringLiteral("ERR"), QStringLiteral("Xung đột cổng: SIYI và Cube FC không thể dùng chung cổng %1").arg(port), 3);
        return;
    }

    _logMavlink(QStringLiteral("SIYI"), QStringLiteral("TX"), QString("[TX CMD] Apply SIYI Link -> %1@%2 bps (FC preserved: %3@%4 bps)").arg(port).arg(baud).arg(fcPort).arg(fcBaud), 5);
    _pendingFcPort    = fcPort;
    _pendingFcBaud    = fcBaud;
    _pendingSiyiPort  = port;
    _pendingSiyiBaud  = baud;
    _pendingCommand   = 44011;
    _waitingTelemetry = false;
    _configStatus     = QStringLiteral("Applying");
    emit configStatusChanged();
    if (!_configTimer) {
        _configTimer = new QTimer(this);
        _configTimer->setSingleShot(true);
        connect(_configTimer, &QTimer::timeout, this, &CompanionController::_configTimedOut);
    }
    _configTimer->start(5000);
    sendLinksConfig(fcBaud, baud, fcPort, port);
    applyConfig(1, true, QStringLiteral("SIYI"));
}

void CompanionController::applyConfig(int subsystemId, bool restartImmediate, const QString& originCategory)
{
    Vehicle* vehicle = _activeVehicle ? _activeVehicle.data() : MultiVehicleManager::instance()->activeVehicle();
    SharedLinkInterfacePtr sharedLink;
    if (vehicle && vehicle->vehicleLinkManager()) {
        sharedLink = vehicle->vehicleLinkManager()->primaryLink().lock();
    }
    if (!sharedLink) {
        const auto links = LinkManager::instance()->links();
        for (const auto& l : links) {
            if (l && l->isConnected()) {
                sharedLink = l;
                break;
            }
        }
    }

    if (!sharedLink) {
        _showToast(QStringLiteral("No active MAVLink link found!"), true);
        return;
    }

    if (!originCategory.isEmpty()) {
        _lastAppliedCategory = originCategory;
    }

    mavlink_message_t msg;
    mavlink_command_long_t cmd{};
    cmd.target_system = vehicle ? vehicle->id() : 1;
    cmd.target_component = kCompanionCompId;
    cmd.command = 44011; // MAV_CMD_THACO_APPLY_CONFIG
    cmd.confirmation = 0;
    cmd.param1 = static_cast<float>(subsystemId);
    cmd.param2 = restartImmediate ? 1.0f : 0.0f;

    mavlink_msg_command_long_encode_chan(
        MAVLinkProtocol::instance()->getSystemId(),
        MAVLinkProtocol::getComponentId(),
        sharedLink->mavlinkChannel(),
        &msg,
        &cmd
    );
    if (vehicle) {
        vehicle->sendMessageOnLinkThreadSafe(sharedLink.get(), msg);
    } else {
        sharedLink->sendMessageThreadSafe(msg);
    }

    _configStatus = "APPLYING";
    emit configStatusChanged();

    // Auto-reset state after 3.5s timeout so button never gets stuck
    QTimer::singleShot(3500, this, [this]() {
        if (_configStatus == "APPLYING") {
            _configStatus = "IDLE";
            emit configStatusChanged();
        }
    });

    QString logTxt = QString("[TX CMD] APPLY_CONFIG -> Subsystem: %1, Restart: %2")
        .arg(subsystemId)
        .arg(restartImmediate ? "Immediate" : "Pending");
    _logMavlink(_lastAppliedCategory.isEmpty() ? "FC" : _lastAppliedCategory, "TX", logTxt, 5);

    _showToast(QString("Sent Apply Config (%1, %2)").arg(_lastAppliedCategory).arg(restartImmediate ? "Restarting" : "Staged"), false);
}

void CompanionController::saveDefaultConfig(int subsystemId)
{
    Vehicle* vehicle = _activeVehicle ? _activeVehicle.data() : MultiVehicleManager::instance()->activeVehicle();
    SharedLinkInterfacePtr sharedLink;
    if (vehicle && vehicle->vehicleLinkManager()) {
        sharedLink = vehicle->vehicleLinkManager()->primaryLink().lock();
    }
    if (!sharedLink) {
        const auto links = LinkManager::instance()->links();
        for (const auto& l : links) {
            if (l && l->isConnected()) {
                sharedLink = l;
                break;
            }
        }
    }
    if (!sharedLink) return;

    mavlink_message_t msg;
    mavlink_command_long_t cmd{};
    cmd.target_system = vehicle ? vehicle->id() : 1;
    cmd.target_component = kCompanionCompId;
    cmd.command = 44010; // MAV_CMD_THACO_SAVE_DEFAULT_CONFIG
    cmd.confirmation = 0;
    cmd.param1 = static_cast<float>(subsystemId);

    mavlink_msg_command_long_encode_chan(
        MAVLinkProtocol::instance()->getSystemId(),
        MAVLinkProtocol::getComponentId(),
        sharedLink->mavlinkChannel(),
        &msg,
        &cmd
    );
    if (vehicle) {
        vehicle->sendMessageOnLinkThreadSafe(sharedLink.get(), msg);
    } else {
        sharedLink->sendMessageThreadSafe(msg);
    }

    QString logTxt = QString("COMMAND_LONG #44010 (SAVE_DEFAULT) -> Subsystem: %1").arg(subsystemId);
    _logMavlink("FC", "TX", logTxt, 5);
    _logMavlink("SIYI", "TX", logTxt, 5);
    _logMavlink("ALL", "TX", logTxt, 5);

    _showToast(QString("Sent Save Default Config (Subsystem %1)").arg(subsystemId), false);
}

void CompanionController::restoreDefaultConfig(int subsystemId)
{
    Vehicle* vehicle = _activeVehicle ? _activeVehicle.data() : MultiVehicleManager::instance()->activeVehicle();
    SharedLinkInterfacePtr sharedLink;
    if (vehicle && vehicle->vehicleLinkManager()) {
        sharedLink = vehicle->vehicleLinkManager()->primaryLink().lock();
    }
    if (!sharedLink) {
        const auto links = LinkManager::instance()->links();
        for (const auto& l : links) {
            if (l && l->isConnected()) {
                sharedLink = l;
                break;
            }
        }
    }
    if (!sharedLink) return;

    mavlink_message_t msg;
    mavlink_command_long_t cmd{};
    cmd.target_system = vehicle ? vehicle->id() : 1;
    cmd.target_component = kCompanionCompId;
    cmd.command = 44012; // MAV_CMD_THACO_RESTORE_DEFAULT_CONFIG
    cmd.confirmation = 0;
    cmd.param1 = static_cast<float>(subsystemId);

    mavlink_msg_command_long_encode_chan(
        MAVLinkProtocol::instance()->getSystemId(),
        MAVLinkProtocol::getComponentId(),
        sharedLink->mavlinkChannel(),
        &msg,
        &cmd
    );
    if (vehicle) {
        vehicle->sendMessageOnLinkThreadSafe(sharedLink.get(), msg);
    } else {
        sharedLink->sendMessageThreadSafe(msg);
    }

    QString logTxt = QString("COMMAND_LONG #44012 (RESTORE_DEFAULT) -> Subsystem: %1").arg(subsystemId);
    _logMavlink("FC", "TX", logTxt, 5);
    _logMavlink("SIYI", "TX", logTxt, 5);
    _logMavlink("ALL", "TX", logTxt, 5);

    _showToast(QString("Sent Restore Default Config (Subsystem %1)").arg(subsystemId), false);
}

void CompanionController::sendCliCommand(const QString& cmdText)
{
    QString trimmed = cmdText.trimmed();
    if (trimmed.isEmpty()) return;

    _logMavlink(QStringLiteral("CLI"), QStringLiteral("TX"), QString("> %1").arg(trimmed), 6);

    QString lower = trimmed.toLower();
    if (lower == "help" || lower == "?") {
        _logMavlink(QStringLiteral("CLI"), QStringLiteral("RX"), QStringLiteral("Commands: status, ping, ports, restart, save, restore, help"), 6);
        return;
    }
    if (lower == "ping") {
        Vehicle* vehicle = _activeVehicle ? _activeVehicle.data() : MultiVehicleManager::instance()->activeVehicle();
        if (!vehicle && !vehicleConnected()) {
            _logMavlink(QStringLiteral("CLI"), QStringLiteral("RX"), QStringLiteral("ERR: No active MAVLink connection"), 3);
        } else {
            int vId = vehicle ? vehicle->id() : 1;
            _logMavlink(QStringLiteral("CLI"), QStringLiteral("RX"), QString("PONG: Vehicle ID %1 online, Companion Comp ID %2 reachable").arg(vId).arg(kCompanionCompId), 6);
        }
        return;
    }
    if (lower == "status") {
        _logMavlink(QStringLiteral("CLI"), QStringLiteral("RX"), QString("SYSTEM: CPU %1% | RAM %2% | Disk %3% | Temp %4C | Uptime %5s")
            .arg(_cpuUsage).arg(_ramUsage).arg(_diskUsage).arg(_cpuTemp).arg(_uptimeS), 6);
        _logMavlink(QStringLiteral("CLI"), QStringLiteral("RX"), QString("LINKS: FC=%1 (%2bps @ %3) | SIYI=%4 (%5bps @ %6)")
            .arg(fcStatus() == 2 ? "ONLINE" : "OFFLINE").arg(fcBaud()).arg(fcPort())
            .arg(siyiStatus() == 2 ? "ONLINE" : "OFFLINE").arg(siyiBaud()).arg(siyiPort()), 6);
        return;
    }
    if (lower == "ports") {
        _logMavlink(QStringLiteral("CLI"), QStringLiteral("RX"), QString("Detected Serial Ports: %1").arg(availablePorts().join(", ")), 6);
        return;
    }
    if (lower == "restart" || lower == "restart agent") {
        applyConfig(0, true);
        _logMavlink(QStringLiteral("CLI"), QStringLiteral("RX"), QStringLiteral("Sent MAVLink agent restart request."), 6);
        return;
    }
    if (lower == "save") {
        saveDefaultConfig(0);
        _logMavlink(QStringLiteral("CLI"), QStringLiteral("RX"), QStringLiteral("Sent Save Default Config command."), 6);
        return;
    }
    if (lower == "restore") {
        restoreDefaultConfig(0);
        _logMavlink(QStringLiteral("CLI"), QStringLiteral("RX"), QStringLiteral("Sent Restore Default Config command."), 6);
        return;
    }

    _logMavlink(QStringLiteral("CLI"), QStringLiteral("RX"), QString("Unknown CLI command: '%1'. Type 'help' for available commands.").arg(trimmed), 4);
}

// ─────────────────────────────────────────────────────────────────────────────
// New public API + test helpers
// ─────────────────────────────────────────────────────────────────────────────

QVariantMap CompanionController::ccTelemetryLinks() const
{
    QVariantMap m;
    if (!_linksService) return m;
    m[QStringLiteral("fc_baudrate")]    = static_cast<qulonglong>(_linksService->fcBaud());
    m[QStringLiteral("siyi_baudrate")]  = static_cast<qulonglong>(_linksService->siyiBaud());
    m[QStringLiteral("fc_bytes_rx")]    = static_cast<qulonglong>(_linksService->fcBytesRx());
    m[QStringLiteral("fc_bytes_tx")]    = static_cast<qulonglong>(_linksService->fcBytesTx());
    m[QStringLiteral("fc_tx_rate")]     = _linksService->fcTxRate();
    m[QStringLiteral("fc_status")]      = _linksService->fcStatus();
    m[QStringLiteral("siyi_status")]    = _linksService->siyiStatus();
    m[QStringLiteral("fc_port")]        = _linksService->fcPort();
    m[QStringLiteral("siyi_port")]      = _linksService->siyiPort();
    const QString proto = _linksService->transportProtocol();
    m[QStringLiteral("transport_type")] = (proto == QStringLiteral("UDP")) ? 2
                                        : (proto == QStringLiteral("TCP")) ? 3 : 1;
    return m;
}

QVariantMap CompanionController::ccTelemetryCamera() const
{
    QVariantMap m;
    m[QStringLiteral("video_width")]      = _videoWidth;
    m[QStringLiteral("video_height")]     = _videoHeight;
    m[QStringLiteral("video_fps")]        = _videoFps;
    m[QStringLiteral("depth_width")]      = _depthWidth;
    m[QStringLiteral("depth_height")]     = _depthHeight;
    m[QStringLiteral("depth_fps")]        = _depthFps;
    m[QStringLiteral("bitrate_kbps")]     = _bitrateKbps;
    m[QStringLiteral("bitrate_max_kbps")] = _bitrateMaxKbps;
    m[QStringLiteral("vbv_buffer_kb")]    = _vbvBufferKb;
    m[QStringLiteral("rotation")]         = _rotation;
    m[QStringLiteral("profile_mode")]     = _profileMode;
    m[QStringLiteral("enable_emitter")]   = _enableEmitter;
    m[QStringLiteral("camera_type")]      = _cameraType;
    m[QStringLiteral("serial_number")]    = _serialNumber;
    m[QStringLiteral("codec")]            = _codec;
    m[QStringLiteral("encoder_mode")]     = _encoderMode;
    m[QStringLiteral("rtsp_url_qgc")]     = _rtspUrlQgc;
    m[QStringLiteral("usb_speed_mode")]   = _usbSpeedMode;
    return m;
}

QVariantMap CompanionController::ccTelemetryNetwork() const
{
    QVariantMap m;
    m[QStringLiteral("eth0_ip")]           = _eth0Ip;
    m[QStringLiteral("eth0_netmask")]      = _eth0Netmask;
    m[QStringLiteral("eth0_status")]       = _eth0Status;
    m[QStringLiteral("eth0_is_static")]    = _eth0IsStatic;
    m[QStringLiteral("wlan0_ip")]          = _wlan0Ip;
    m[QStringLiteral("wlan0_netmask")]     = _wlan0Netmask;
    m[QStringLiteral("wlan0_status")]      = _wlan0Status;
    m[QStringLiteral("wlan0_ssid")]        = _wlan0Ssid;
    m[QStringLiteral("wlan0_rssi")]        = _wlan0Rssi;
    m[QStringLiteral("wlan0_dhcp")]        = _wlan0Dhcp;
    m[QStringLiteral("ap_ip")]             = _apIp;
    m[QStringLiteral("ap_netmask")]        = _apNetmask;
    m[QStringLiteral("ap_ssid")]           = _apSsid;
    m[QStringLiteral("ap_channel")]        = _apChannel;
    m[QStringLiteral("ap_client_count")]   = _apClientCount;
    m[QStringLiteral("ap_status")]         = _apStatus;
    m[QStringLiteral("ap_hw_mode")]        = _apHwMode;
    m[QStringLiteral("dnsmasq_status")]    = _dnsmasqStatus;
    m[QStringLiteral("ap_ieee80211n")]     = _apIeee80211n;
    m[QStringLiteral("ap_wmm_enabled")]    = _apWmmEnabled;
    m[QStringLiteral("ap_wpa")]            = _apWpa;
    m[QStringLiteral("ap_wpa_passphrase")] = _apWpaPassphrase;
    m[QStringLiteral("ap_key_mgmt")]       = _apKeyMgmt;
    return m;
}

QVariantMap CompanionController::ccTelemetryVision() const
{
    QVariantMap m;
    m[QStringLiteral("confidence_thresh")] = _confidenceThresh;
    m[QStringLiteral("inference_fps")]     = _inferenceFps;
    m[QStringLiteral("input_width")]       = _visionInputWidth;
    m[QStringLiteral("input_height")]      = _visionInputHeight;
    m[QStringLiteral("video_fps")]         = _visionVideoFps;
    m[QStringLiteral("detections_count")]  = _detectionsCount;
    m[QStringLiteral("status_flags")]      = _visionStatusFlags;
    m[QStringLiteral("model_name")]        = _modelName;
    m[QStringLiteral("input_source")]      = _inputSource;
    return m;
}

QVariantMap CompanionController::ccTelemetrySystem() const
{
    QVariantMap m;
    m[QStringLiteral("cpu_usage")]       = _cpuUsage;
    m[QStringLiteral("ram_usage")]       = _ramUsage;
    m[QStringLiteral("disk_usage")]      = _diskUsage;
    m[QStringLiteral("cpu_temp")]        = _cpuTemp;
    m[QStringLiteral("system_uptime_s")] = static_cast<qulonglong>(_uptimeS);
    return m;
}

bool CompanionController::logMatchesFilter(const QString& prefix, const QString& category, const QString& text) const
{
    Q_UNUSED(category);
    return text.startsWith(prefix);
}

void CompanionController::applyLinksConfig(const QString& fcPort, int fcBaud,
                                            const QString& siyiPort, int siyiBaud)
{
    if (fcPort.toUtf8().size() > 15 || siyiPort.toUtf8().size() > 15) {
        _configMessage = QStringLiteral("UART port must fit within 15 UTF-8 bytes");
        _configStatus  = QStringLiteral("Failed");
        emit configStatusChanged();
        return;
    }
    if (_pendingCommand || _waitingTelemetry) {
        return;
    }

    _pendingFcPort   = fcPort;
    _pendingFcBaud   = fcBaud;
    _pendingSiyiPort = siyiPort;
    _pendingSiyiBaud = siyiBaud;
    _pendingCommand  = 44011;
    _waitingTelemetry = false;
    _configMessage.clear();

    if (_linksService) {
        _linksService->sendLinksConfig(_activeVehicle ? _activeVehicle.data() : nullptr,
                                       fcBaud, siyiBaud, fcPort, siyiPort);
    }

    applyConfig(1, true, QStringLiteral("FC"));

    _configStatus = QStringLiteral("Applying");
    emit configStatusChanged();

    if (!_configTimer) {
        _configTimer = new QTimer(this);
        _configTimer->setSingleShot(true);
        connect(_configTimer, &QTimer::timeout, this, &CompanionController::_configTimedOut);
    }
    _configTimer->start(5000);
}

void CompanionController::saveLinksConfig()
{
    if (_pendingCommand || _waitingTelemetry) {
        return;
    }
    _pendingCommand   = 44010;
    _waitingTelemetry = false;
    saveDefaultConfig(1);

    _configStatus = QStringLiteral("Applying");
    emit configStatusChanged();

    if (!_confirmTimer) {
        _confirmTimer = new QTimer(this);
        _confirmTimer->setSingleShot(true);
        connect(_confirmTimer, &QTimer::timeout, this, &CompanionController::_confirmationTimedOut);
    }
    _confirmTimer->start(5000);
}

void CompanionController::_configTimedOut()
{
    _pendingCommand = 0;
    _waitingTelemetry = false;
    if (_configStatus == QStringLiteral("Applying")) {
        _configStatus = QStringLiteral("Timeout");
        emit configStatusChanged();
    }
}

void CompanionController::_confirmationTimedOut()
{
    _pendingCommand = 0;
    _waitingTelemetry = false;
    if (_configStatus == QStringLiteral("WaitingTelemetry") ||
        _configStatus == QStringLiteral("Applying")) {
        _configStatus = QStringLiteral("Timeout");
        emit configStatusChanged();
    }
}

void CompanionController::resetForTest()
{
    _sourceSystemId    = -1;
    _sourceComponentId = -1;
    _linksStale        = false;
    _pendingCommand    = 0;
    _waitingTelemetry  = false;
    _clearTelemetryState();
}

void CompanionController::forceStaleForTest()
{
    _linksStale = true;
    if (_telemetryWatchdog) _telemetryWatchdog->stop();
}

void CompanionController::_checkTelemetryConfirmation()
{
    if (!_waitingTelemetry || !linksReceived() || _linksStale) {
        return;
    }
    if (_linksService->fcPort() != _pendingFcPort ||
        _linksService->fcBaud() != _pendingFcBaud ||
        _linksService->siyiPort() != _pendingSiyiPort ||
        _linksService->siyiBaud() != _pendingSiyiBaud) {
        return;
    }
    if (_confirmTimer) {
        _confirmTimer->stop();
    }
    _waitingTelemetry = false;
    _configStatus = QStringLiteral("Success");
    _configMessage = QStringLiteral("UART configuration confirmed by telemetry");
    emit configStatusChanged();
}
