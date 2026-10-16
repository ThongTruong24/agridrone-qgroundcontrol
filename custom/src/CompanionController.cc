#include "CompanionController.h"

#include <QTimer>
#include <QtCore/QByteArray>
#include <QtCore/QDebug>
#include <QtCore/QPointer>
#include <QtCore/QRegularExpression>
#include <QtCore/QUrl>
#include <cmath>

#include "AppMessages.h"
#include "Companion/CompanionProtocol.h"
#include "LinkManager.h"
#include "MAVLinkProtocol.h"
#include "MultiVehicleManager.h"
#include "VehicleLinkManager.h"
#include "VideoManager.h"

static constexpr uint8_t kCompanionCompId = 191; // MAV_COMP_ID_ONBOARD_COMPUTER

namespace {
// Time budgets (ms). Unit tests use short ones so they never wait for the real values.
int firstAckMs()
{
    return QGC::runningUnitTests() ? 500 : 2000;
}

int finalAckMs()
{
    return QGC::runningUnitTests() ? 1500 : 8000;
}

int totalMs(const QString& group)
{
    const bool test = QGC::runningUnitTests();
    if (group == "links")
        return test ? 4000 : 15000;
    return test ? 6000 : 25000; // hotspot: reload + AP wait, and the same again for a rollback
}
// Camera/Vision work has no deadline (the CC keeps sending IN_PROGRESS and status lines); only this much silence ends the wait.
int silenceMs() { return QGC::runningUnitTests() ? 300 : 3000; }
// After a hotspot APPLY's reconnect, how long the AP has to show the candidate in its telemetry.
int hotspotConfirmWindowMs() { return QGC::runningUnitTests() ? 600 : 20000; }
bool streamGroup(const QString& group) { return group == QLatin1String("camera") || group == QLatin1String("vision"); }
const QString kModeId = QStringLiteral("CC_STREAM_MODE");

QString modeName(int mode)
{
    return mode == 0 ? QStringLiteral("Off") : mode == 1 ? QStringLiteral("Video") : QStringLiteral("Vision");
}

QString groupPrefix(const QString& group)
{
    if (group == "links")
        return QStringLiteral("Links: ");
    return QStringLiteral("Hotspot: ");
}

// What the CC is doing at each reported progress value (same numbers as config_manager's report_stage).
QString stageText(int subsystem, int progress)
{
    if (subsystem == 1) {
        switch (progress) {
        case 20: return QObject::tr("Kiểm tra cấu hình link");
        case 50: return QObject::tr("Router mở lại cổng nối tiếp");
        case 90: return QObject::tr("Đã áp dụng");
        }
    } else if (subsystem == 3) {
        switch (progress) {
        case 15: return QObject::tr("Kiểm tra cấu hình hotspot");
        case 35: return QObject::tr("Khởi động lại dịch vụ mạng");
        case 60: return QObject::tr("Chờ điểm phát WiFi lên");
        case 90: return QObject::tr("Đã áp dụng");
        }
    }
    return QStringLiteral("%1%").arg(progress);
}

QString lockReason(bool locked) { return locked ? QObject::tr("Companion đang khóa cấu hình (máy bay ARM)") : QString(); }
QString catalogError() { return QObject::tr("Chưa tải được danh sách tham số Companion"); }

// The AP telemetry (hotspotState) shows the candidate's SSID, address and DHCP; the passphrase is never compared.
bool apShows(const QVariantMap& active, const QMap<QString, QVariant>& candidate)
{
    return active.value("ssid").toString() == candidate.value("CC_AP_SSID").toString() &&
           active.value("ipCidr").toString() == candidate.value("CC_AP_IP").toString() &&
           active.value("dhcpEnabled").toBool() == candidate.value("CC_AP_DHCP").toBool();
}
}  // namespace

CompanionController::CompanionController(QObject* parent) : QObject(parent)
{
    _toolbarClock.start();
    _toolbarOperationTimer = new QTimer(this);
    _toolbarOperationTimer->setSingleShot(true);
    connect(_toolbarOperationTimer, &QTimer::timeout, this, [this]() { toolbarTimedOut(); });
    _setAckTimer = new QTimer(this);
    _setAckTimer->setSingleShot(true);
    connect(_setAckTimer, &QTimer::timeout, this, [this]() { setAckTimedOut(); });
    auto freshnessTimer = new QTimer(this);
    freshnessTimer->setInterval(500);
    connect(freshnessTimer, &QTimer::timeout, this, [this]() {
        if (!_ccLost && _toolbarReceived.contains("heartbeat") && !companionState().value("connected").toBool()) {
            _ccLost = true;
            onCompanionLost();
        }
        checkToolbarEffect();
        emit toolbarStateChanged();
    });
    freshnessTimer->start();
    // SOLID: Decoupled services
    _logService = new CompanionLogService(this);
    _configService = new CompanionConfigService(this);
    _linksService = new CompanionLinksService(this);
    _paramService = new CompanionParamService(this);
    _dispatcher = new CompanionMavlinkDispatcher(this);

    // Register handlers to dispatcher (SOLID: Open/Closed & Dependency Inversion)
    _dispatcher->registerHandler(_linksService);
    _dispatcher->registerHandler(_paramService);
    _dispatcher->registerHandler(_configService);
    connect(_configService, &CompanionConfigService::changed, this, &CompanionController::configStatusChanged);
    connect(_paramService, &CompanionParamService::logMessage, _logService, &CompanionLogService::logMavlink);
    connect(_linksService, &CompanionLinksService::logMessage, _logService, &CompanionLogService::logMavlink);
    connect(_logService, &CompanionLogService::logEntriesChanged, this, &CompanionController::logEntriesChanged);
    connect(_logService, &CompanionLogService::logMessageAdded, this, &CompanionController::mavlinkLogMessage);

    // Connect service signals to facade
    connect(_linksService, &CompanionLinksService::linksChanged, this, [this]() {
        emit linksChanged();
        emit toolbarStateChanged();
        emit vehicleConnectedChanged();
    });
    connect(_paramService, &CompanionParamService::paramListChanged, this, [this]() {
        syncPlayerToMode();
        checkReadbackAfterReconnect();
        emit availablePortsChanged(); // CC_SER_PORTS is a parameter
        emit toolbarStateChanged();
    });
    connect(_paramService, &CompanionParamService::isLoadingChanged, this, &CompanionController::ccParametersLoadingChanged);
    connect(_paramService, &CompanionParamService::parameterAckInProgress, this, [this](const QString& name) {
        // Nothing pending for it: a keepalive that trails the final ACK (the CC may send one just after FAILED).
        if (!_toolbarOperation || _toolbarOperation->awaiting != name)
            return;
        auto& op = *_toolbarOperation;
        // The replaced op's keepalive may still come before its FAILED; the CC sends that FAILED before the new op's
        // IN_PROGRESS, so only a second IN_PROGRESS proves the FAILED is not coming.
        if (++op.inProgressAcks >= 2)
            op.expectCancelAck = false;
        if (!op.sawInProgress)
            addOperationStep("rx", QStringLiteral("ACK IN_PROGRESS"), "done");  // keepalives repeat every 500 ms
        op.sawInProgress = true;
        _setAckTimer->start(streamGroup(op.group) ? silenceMs() : finalAckMs());
    });
    connect(_paramService, &CompanionParamService::parameterSaved, this,
            [this](const QString& name, bool success, const QString& msg) {
        toolbarParameterSaved(name, success);
        _showToast(msg, !success);
    });
    // Keep GCS values in step with the CC (an owner restart or an SSH change moves them).
    _ccSyncTimer = new QTimer(this);
    _ccSyncTimer->setInterval(30000);
    connect(_ccSyncTimer, &QTimer::timeout, this, [this]() {
        if (companionState().value("connected").toBool() && !_paramService->isLoading() && !_toolbarOperation) {
            _paramService->requestParameters();
        }
    });
    _ccSyncTimer->start();

    _aiVisionControlTimer.setInterval(1000);
    connect(&_aiVisionControlTimer, &QTimer::timeout, this, &CompanionController::_sendAiVisionControl);
    _aiVisionControlTimer.start();

    MultiVehicleManager* const multiVehicleManager = MultiVehicleManager::instance();
    connect(multiVehicleManager, &MultiVehicleManager::activeVehicleChanged, this,
            &CompanionController::_setActiveVehicle);
    _setActiveVehicle(multiVehicleManager->activeVehicle());

    // Ingest all MAVLink messages directly via MAVLinkProtocol (same as MAVLinkInspector)
    connect(MAVLinkProtocol::instance(), &MAVLinkProtocol::messageReceived,
            this, [this](LinkInterface* link, const mavlink_message_t& message) { _onMavlinkMessageReceived(message, link); });

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
    onCompanionLost(); // the watchdog and the heartbeat follow one rule
    _toolbarReceived.clear();
    _streamTelemetry.clear();
    _networkConfigVersion = 0;
    _companionName.clear();
    emit toolbarStateChanged();
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
    _lastTriggerId = 0;
    _lastTriggerTimeBootMs = 0;

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
    if (_activeVehicle == vehicle)
        return;

    disconnect(_vehicleDestroyedConnection);

    // The CC session does not hang on the FC's Vehicle: it ends with it only when the CC heartbeat is stale too. A Vehicle of
    // another system ends it at once: QGC never configures a CC that is not the active Vehicle's (spec §5, ADR-0001).
    const bool otherSystem = vehicle && _sourceSystemId >= 0 && vehicle->id() != _sourceSystemId;
    if (otherSystem || (_activeVehicle && !companionState().value("connected").toBool())) {
        _sourceSystemId    = -1;
        _sourceComponentId = -1;
        _ccLink.reset();
        _paramService->setTransport({}); // before the clear: its failed-op re-read must not reach the old CC
        ++_ccSessionEpoch;
        if (otherSystem) {
            finishToolbarOperation(tr("Mất kết nối Companion")); // that CC cannot come back while this Vehicle is active
            _unsaved.clear(); // the next CC locked is this Vehicle's, not the one that applied it
            _saveBlocked.clear();
        }
        _clearTelemetryState();
    }

    _activeVehicle = vehicle;
    _sendAiVisionControl();
    emit vehicleConnectedChanged();
    emit vehicleAvailableChanged();
}

CompanionTransport CompanionController::transport()
{
    const auto link = _ccLink.lock();
    return {_sourceSystemId, link ? link->mavlinkChannel() : uint8_t(0),
            [this](const mavlink_message_t& m) { return sendToCompanion(m); }};
}

void CompanionController::setBoundingBoxEnabled(bool enabled)
{
    if (_boundingBoxEnabled == enabled) {
        return;
    }

    _boundingBoxEnabled = enabled;
    if (!enabled) {
        _trackingEnabled = false;
        _followingEnabled = false;
    }
    emit aiVisionControlChanged();
    _sendAiVisionControl();
}

void CompanionController::setTrackingEnabled(bool enabled)
{
    if ((enabled && !_boundingBoxEnabled) || (_trackingEnabled == enabled)) {
        return;
    }

    _trackingEnabled = enabled;
    emit aiVisionControlChanged();
    _sendAiVisionControl();
}

void CompanionController::setFollowingEnabled(bool enabled)
{
    if ((enabled && !_boundingBoxEnabled) || (_followingEnabled == enabled)) {
        return;
    }

    _followingEnabled = enabled;
    emit aiVisionControlChanged();
    _sendAiVisionControl();
}

void CompanionController::_sendAiVisionControl()
{
    (void) Companion::sendAiVisionControl(_activeVehicle.data(), _boundingBoxEnabled, _trackingEnabled,
                                          _followingEnabled);
}

bool CompanionController::sendAiVisionTrackPoint(double normalizedX, double normalizedY, double normalizedRadius)
{
    if (!_boundingBoxEnabled || !std::isfinite(normalizedX) || !std::isfinite(normalizedY) ||
        !std::isfinite(normalizedRadius) || normalizedX < 0.0 || normalizedX > 1.0 || normalizedY < 0.0 ||
        normalizedY > 1.0 || normalizedRadius < 0.0 || normalizedRadius > 1.0 || !_activeVehicle) {
        return false;
    }

    return Companion::sendTrackPoint(_activeVehicle.data(), normalizedX, normalizedY, normalizedRadius);
}

void CompanionController::_logMavlink(const QString& category, const QString& direction, const QString& message,
                                      int severity)
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

bool CompanionController::sendToCompanion(const mavlink_message_t& message)
{
    if (_activeVehicle && _activeVehicle->id() != _sourceSystemId) return false; // never another drone's CC (spec §5)
    if (_sendHook) return _sendHook(message);
    return Companion::sendMessage(nullptr, _ccLink.lock(), message); // no Vehicle needed (spec §5)
}

bool CompanionController::sendCompanionCommand(uint16_t command, float param1, float param2)
{
    if (_sourceSystemId < 0) return false;
    const auto link = _ccLink.lock();
    mavlink_message_t msg;
    mavlink_msg_command_long_pack_chan(MAVLinkProtocol::instance()->getSystemId(), MAVLinkProtocol::getComponentId(),
        link ? link->mavlinkChannel() : 0, &msg, static_cast<uint8_t>(_sourceSystemId), kCompanionCompId, command, 0,
        param1, param2, 0, 0, 0, 0, 0);
    return sendToCompanion(msg); // sent once: a lost ACK is settled by telemetry/readback, never by a resend
}

void CompanionController::setSendHookForTest(std::function<bool(const mavlink_message_t&)> hook)
{
    _sendHook = std::move(hook);
    if (_sourceSystemId > 0) _paramService->setTransport(transport());
}

void CompanionController::onCompanionLost()
{
    if (!_toolbarOperation) return;
    auto& op = *_toolbarOperation;
    if (op.command != MAV_CMD_THACO_APPLY_CONFIG) { finishToolbarOperation(tr("Mất kết nối Companion")); return; }
    // The APPLY restarts the router/AP that may carry this very link: wait without deadline, then read back (spec §5).
    _setAckTimer->stop();
    _toolbarOperationTimer->stop(); // also a hotspot's confirm window: it starts again when the CC is back
    if (op.waitingReconnect) return;
    op.resultSeen = true;
    op.waitingReconnect = true;
    settleWaitingSteps("done");
    addOperationStep("info", tr("Chưa xác nhận, đang chờ kết nối lại"), "waiting");
}

void CompanionController::onCompanionSessionStarted()
{
    if (!_toolbarOperation || !_toolbarOperation->waitingReconnect) return;
    auto& op = *_toolbarOperation;
    // Not the CC that got the APPLY: that one may well have applied it, so nothing is known.
    if (_sourceSystemId != op.systemId) { endUnconfirmed(tr("CC khác đã kết nối lại")); return; }
    op.epoch = _ccSessionEpoch;
    op.readbackRequested = true;
    // Only values read in this session count, not what the SET ACKs left before the loss.
    for (auto it = op.effectCandidate.cbegin(); it != op.effectCandidate.cend(); ++it) _paramService->forgetConfirmed(it.key());
    settleWaitingSteps("done");
    addOperationStep("tx", tr("Đọc lại tham số"), "waiting");
    if (op.subsystem != 3) return;
    // The CC writes a hotspot candidate to its active file before verifying it and keeps it (and its stage) when the AP
    // fails, so the readback matches either way: only the AP's telemetry newer than now confirms it, within a window.
    op.waitingEffect = true;
    op.effectAfter = _toolbarClock.elapsed();
    _toolbarOperationTimer->start(hotspotConfirmWindowMs());
    addOperationStep("info", tr("Chờ telemetry WiFi xác nhận cấu hình mới"), "waiting");
}

void CompanionController::checkReadbackAfterReconnect()
{
    if (!_toolbarOperation || !_toolbarOperation->readbackRequested) return;
    const auto& op = *_toolbarOperation;
    for (auto it = op.effectCandidate.cbegin(); it != op.effectCandidate.cend(); ++it) {
        if (_paramService->isSecret(it.key())) continue; // never echoed back
        const auto value = _paramService->confirmedValue(it.key());
        if (!value.isValid()) return; // not read back yet
        if (value.toString() != it.value().toString()) { finishToolbarOperation(tr("Không áp dụng được")); return; }
    }
    if (op.subsystem == 3) return; // a match proves nothing for the hotspot: its AP telemetry decides (checkToolbarEffect)
    // Without the CC's IN_PROGRESS or ACCEPTED (waitingEffect) nothing proves it got the APPLY: the values may be its stage alone.
    if (op.lastApplyProgress < 0 && !op.waitingEffect) { endUnconfirmed(tr("Chưa xác nhận")); return; }
    finishToolbarOperation();
}

void CompanionController::cancelCompanionWait()
{
    if (_toolbarOperation && _toolbarOperation->waitingReconnect) endUnconfirmed(tr("Đã hủy chờ"));
}

bool CompanionController::configLocked() const
{
    const bool fresh = _toolbarReceived.contains("system") && _toolbarClock.elapsed() - _toolbarReceived.value("system") <= 3500;
    return (fresh && (_systemStatus & CC_SYS_STATUS_CONFIG_LOCKED)) || (_activeVehicle && _activeVehicle->armed());
}

bool CompanionController::saveCompanionGroup(const QString& group)
{
    const int subsystem = group == QLatin1String("links") ? 1 : group == QLatin1String("hotspot") ? 3 : 0;
    // Only what the group's last APPLY confirmed: SAVE persists whatever the CC runs now (spec §7).
    if (!subsystem || !_unsaved.value(group) || _saveBlocked.value(group) || _toolbarOperation ||
        !companionState().value("connected").toBool() || configLocked()) return false;
    ToolbarOperation op;
    op.group = group; op.subsystem = subsystem; op.origin = QStringLiteral("companion");
    op.epoch = _ccSessionEpoch; op.systemId = _sourceSystemId; op.startedMs = _toolbarClock.elapsed();
    op.applySent = true; op.waitingCommand = true; op.command = MAV_CMD_THACO_SAVE_DEFAULT_CONFIG;
    _toolbarErrors.remove(group);
    _toolbarOperation = op;
    addOperationStep("tx", QStringLiteral("SAVE %1").arg(group), "waiting");
    if (!sendCompanionCommand(MAV_CMD_THACO_SAVE_DEFAULT_CONFIG, subsystem)) {
        finishToolbarOperation(tr("Không gửi được tới Companion"));
        return false;
    }
    _setAckTimer->start(silenceMs());
    return true;
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

void CompanionController::_onMavlinkMessageReceived(const mavlink_message_t& message, LinkInterface* receivedOn)
{
    if (!_activeVehicle) {
        Vehicle* active = MultiVehicleManager::instance()->activeVehicle();
        if (active) {
            _setActiveVehicle(active);
        }
    }

    if (message.msgid == 32000) {  // MAVLINK_MSG_ID_THACO_EXTERNAL_XYZ_TRIGGER
        if (_activeVehicle && message.sysid != _activeVehicle->id()) return;
        mavlink_thaco_external_xyz_trigger_t trigger{};
        mavlink_msg_thaco_external_xyz_trigger_decode(&message, &trigger);
        _lastTriggerId = trigger.trigger_id;
        _lastTriggerTimeBootMs = trigger.time_boot_ms;
        _hasMissionTelemetry = true;
        emit missionChanged();
        _logMavlink(
            QStringLiteral("MISSION"), QStringLiteral("RX"),
            QStringLiteral("MISSION: XYZ trigger %1 at %2 ms").arg(trigger.trigger_id).arg(trigger.time_boot_ms), 6);
        return;
    }

    if (message.msgid == 253) { // MAVLINK_MSG_ID_STATUSTEXT
        // Only the group of the running operation: "Links: ", "Hotspot: ", "Camera: ", "Vision: ".
        if (message.compid != kCompanionCompId || (_sourceSystemId >= 0 ? message.sysid != _sourceSystemId : !_activeVehicle || message.sysid != _activeVehicle->id())) return;
        mavlink_statustext_t st;
        mavlink_msg_statustext_decode(&message, &st);
        const QString text = QString::fromUtf8(st.text, qstrnlen(st.text, sizeof(st.text))).trimmed();
        if (!text.isEmpty())
            _logMavlink(QStringLiteral("ALL"), QStringLiteral("RX"), text, st.severity);
        if (!_toolbarOperation)
            return;
        auto& op = *_toolbarOperation;
        const bool stream = streamGroup(op.group);
        if (stream ? !(text.startsWith(QStringLiteral("Camera: ")) || text.startsWith(QStringLiteral("Vision: ")))
                   : !text.startsWith(groupPrefix(op.group)))
            return;
        op.lastText = text;
        if (stream) {
            static const QRegularExpression stepPattern(QStringLiteral("\\[(\\d+)/(\\d+)\\]"));
            const auto match = stepPattern.match(text);
            if (match.hasMatch() && match.captured(2).toInt() > 0) {
                op.progress = qBound(0.0, match.captured(1).toDouble() / match.captured(2).toInt(), 1.0);
            }
            if (!op.awaiting.isEmpty())
                _setAckTimer->start(silenceMs());
        }
        addOperationStep("info", text, "done");
        return;
    }

    // Telemetry messages (42010..42014): must come from Companion Computer (compid 191)
    if (message.compid != kCompanionCompId) {
        return;
    }

    if (_sourceComponentId < 0) {
        // While a Vehicle is active only its own CC may lock; without one the first CC heard does (ADR-0001).
        if (_activeVehicle && message.sysid != _activeVehicle->id()) return;
        if (message.sysid != _lastCompanionSystemId) { _unsaved.clear(); _saveBlocked.clear(); } // what is unsaved is that CC's
        _lastCompanionSystemId = message.sysid;
        _sourceSystemId    = message.sysid;
        _sourceComponentId = message.compid;
    } else if (message.sysid != _sourceSystemId || message.compid != _sourceComponentId) {
        return;
    }

    if (message.msgid == MAVLINK_MSG_ID_HEARTBEAT) {
        if (receivedOn) _ccLink = LinkManager::instance()->sharedLinkInterfacePointerForLink(receivedOn);
        const bool sessionStart = !companionState().value("connected").toBool(); // first heartbeat, or back after > 3.5 s
        _toolbarReceived["heartbeat"] = _toolbarClock.elapsed();
        if (sessionStart) {
            _ccLost = false;
            ++_ccSessionEpoch;
            _paramService->setTransport(transport());
            if (!_paramService->isLoading()) _paramService->requestParameters();
            onCompanionSessionStarted();
        }
        emit toolbarStateChanged();
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

    if (message.msgid == MAVLINK_MSG_ID_COMMAND_ACK) {
        // QGC's own client for the command it sent the CC (no MavCommandQueue: no resend).
        if (_toolbarOperation && _toolbarOperation->applySent && !_toolbarOperation->resultSeen) {
            mavlink_command_ack_t ack;
            mavlink_msg_command_ack_decode(&message, &ack);
            if (ack.command != _toolbarOperation->command) return;
            if (ack.result == MAV_RESULT_IN_PROGRESS) {
                onApplyProgress(ack.progress);
                _setAckTimer->start(silenceMs());
            } else {
                _setAckTimer->stop();
                onApplyResult(ack.result, false);
            }
        }
        return; // no handler or telemetry case reads a COMMAND_ACK
    }

    if (message.msgid == MAVLINK_MSG_ID_CC_SERIAL_LINK) {
        mavlink_cc_serial_link_t link{};
        mavlink_msg_cc_serial_link_decode(&message, &link);
        _toolbarReceived[QString("link:%1").arg(link.link_index)] = _toolbarClock.elapsed();
    }
    // Dispatch to registered handlers first (Links, etc.)
    if (_dispatcher && _dispatcher->dispatchMessage(message)) {
        return;
    }

    switch (message.msgid) {
        case 42011: { // CC_TELEMETRY_CAMERA
        mavlink_cc_telemetry_camera_t cam;
        mavlink_msg_cc_telemetry_camera_decode(&message, &cam);

        _toolbarReceived["camera"] = _toolbarClock.elapsed();
            _streamTelemetry["camera"] = {{"version", cam.config_version},
                                          {"enabled", bool(cam.fpv_enabled)},
                                          {"publishStatus", cam.publish_status},
                                          {"supportedCodecs", cam.supported_codecs}};
        _hasCameraTelemetry = true;
        _camStatus = cam.camera_status;
        _camName = QString::fromUtf8(cam.camera_name, qstrnlen(cam.camera_name, sizeof(cam.camera_name)));
        _cameraType = QString::fromUtf8(cam.camera_type, qstrnlen(cam.camera_type, sizeof(cam.camera_type)));
            _connectionPort =
                QString::fromUtf8(cam.connection_port, qstrnlen(cam.connection_port, sizeof(cam.connection_port)));
            _serialNumber =
                QString::fromUtf8(cam.serial_number, qstrnlen(cam.serial_number, sizeof(cam.serial_number)));
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
            _rtspUrlController = QString::fromUtf8(cam.rtsp_url_controller,
                                                   qstrnlen(cam.rtsp_url_controller, sizeof(cam.rtsp_url_controller)));
            _rtspUrlLaptop =
                QString::fromUtf8(cam.rtsp_url_laptop, qstrnlen(cam.rtsp_url_laptop, sizeof(cam.rtsp_url_laptop)));
        _codec = QString::fromUtf8(cam.codec, qstrnlen(cam.codec, sizeof(cam.codec)));
        _encoderMode = QString::fromUtf8(cam.encoder_mode, qstrnlen(cam.encoder_mode, sizeof(cam.encoder_mode)));
        _isDefaultCam = cam.is_default != 0;
        _profileMode = cam.profile_mode;
        _enableEmitter = cam.enable_emitter;
        _cameraStatus = cam.camera_status;
        _errorCode = cam.error_code;
        _usbSpeedMode = cam.usb_speed_mode;

        emit cameraChanged();
        emit toolbarStateChanged();
        break;
    }

    case 42012: { // CC_TELEMETRY_NETWORK
        mavlink_cc_telemetry_network_t net;
        mavlink_msg_cc_telemetry_network_decode(&message, &net);

        _toolbarReceived["network"] = _toolbarClock.elapsed();
        _networkConfigVersion = net.config_version;
        _apPrefixLength = net.ap_prefix_length;
        _apDhcpEnabled = net.ap_dhcp_enabled;
        _hasNetworkTelemetry = true;
        _eth0Ip = QString::fromUtf8(net.eth0_ip, qstrnlen(net.eth0_ip, sizeof(net.eth0_ip)));
        _eth0Netmask = QString::fromUtf8(net.eth0_netmask, qstrnlen(net.eth0_netmask, sizeof(net.eth0_netmask)));
        _eth0Status = net.eth0_status;
        _eth0IsStatic = net.eth0_is_static;
        _eth0RxKb = net.eth0_rx_kb;
        _eth0TxKb = net.eth0_tx_kb;
        _wlan0Ip = QString::fromUtf8(net.wlan0_ip, qstrnlen(net.wlan0_ip, sizeof(net.wlan0_ip)));
            _wlan0Netmask =
                QString::fromUtf8(net.wlan0_netmask, qstrnlen(net.wlan0_netmask, sizeof(net.wlan0_netmask)));
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
            _apKeyMgmt = QString::fromUtf8(net.ap_key_mgmt, qstrnlen(net.ap_key_mgmt, sizeof(net.ap_key_mgmt)));

        checkToolbarEffect(); // a hotspot APPLY waits for this
        emit networkChanged();
        emit toolbarStateChanged();
        break;
    }

    case 42013: { // CC_TELEMETRY_VISION
        mavlink_cc_telemetry_vision_t vis;
        mavlink_msg_cc_telemetry_vision_decode(&message, &vis);

        _toolbarReceived["vision"] = _toolbarClock.elapsed();
        // The Vision stream is encoded by the camera now: its publisher fields are always 0 and not used.
        _streamTelemetry["vision"] = {{"version", vis.config_version}, {"enabled", bool(vis.enabled)}};
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
        emit toolbarStateChanged();
        break;
    }

    case 42014: { // CC_TELEMETRY_SYSTEM
        mavlink_cc_telemetry_system_t sys;
        mavlink_msg_cc_telemetry_system_decode(&message, &sys);

        const bool prevConn = vehicleConnected();
        _toolbarReceived["system"] = _toolbarClock.elapsed();
        _companionName = Companion::fromMavString(sys.companion_name);
        _hasSystemTelemetry = true;
        _cpuUsage = sys.cpu_usage;
        _ramUsage = sys.ram_usage;
        _diskUsage = sys.disk_usage;
        _cpuTemp = sys.cpu_temp;
        _uptimeS = sys.system_uptime_s;
        _systemStatus = sys.system_status;

        emit systemChanged();
        emit toolbarStateChanged();
        if (prevConn != vehicleConnected()) {
            emit vehicleConnectedChanged();
        }
        break;
    }

    default:
        break;
    }
}

CompanionController::~CompanionController() = default;

// ─────────────────────────────────────────────────────────────────────────────
// New public API + test helpers
// ─────────────────────────────────────────────────────────────────────────────

QStringList CompanionController::availablePorts() const
{
    const auto value = _paramService->confirmedValue(QStringLiteral("CC_SER_PORTS"));
    return value.isValid() ? value.toString().split(',', Qt::SkipEmptyParts) : QStringList(); // milestone B: metadata enum
}

QStringList CompanionController::baudOptions(int index) const
{
    return _paramService->options(QStringLiteral("CC_L%1_BAUD").arg(index)); // shortcut: bundled json until milestone B
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

void CompanionController::resetForTest()
{
    _sourceSystemId    = -1;
    _sourceComponentId = -1;
    _linksStale        = false;
    _clearTelemetryState();
}

void CompanionController::setReceivedAgeForTest(const QString& key, qint64 ageMs)
{
    _toolbarReceived[key] = _toolbarClock.elapsed() - ageMs;
}

void CompanionController::forceStaleForTest()
{
    _linksStale = true;
    if (_telemetryWatchdog)
        _telemetryWatchdog->stop();
}

QVariantMap CompanionController::streamState(const QString& stream) const
{
    auto state = _streamTelemetry.value(stream);
    const bool fresh =
        _toolbarReceived.contains(stream) && _toolbarClock.elapsed() - _toolbarReceived.value(stream) <= 3500;
    state["available"] = fresh && state.value("version").toInt() == 1;
    state["fresh"] = fresh;
    state["pending"] = _toolbarOperation && _toolbarOperation->group == stream;
    state["error"] = _toolbarErrors.value(stream);
    state["operation"] = operationState(QStringLiteral("stream"));
    state["reason"] = streamReason(stream);
    const bool camera = stream == "camera";
    auto value = [this](const QString& id) { return _paramService->confirmedValue(id); };
    state["configuredUrl"] = value(camera ? "CC_CAM_PUB_URL" : "CC_VIS_OUT_URL");
    if (!camera) {
        state["confidence"] = value("CC_VIS_CONF");
        state["supportedCodecs"] =
            _streamTelemetry.value("camera").value("supportedCodecs");  // the camera encodes both streams
    }
    state["configured"] = QVariantMap{{"resolution", value(camera ? "CC_CAM_RES" : "CC_VIS_RES")},
                                      {"fps", value(camera ? "CC_CAM_FPS" : "CC_VIS_FPS")},
                                      {"bitrate", value(camera ? "CC_CAM_BR" : "CC_VIS_BR")},
        {"codec", value(camera ? "CC_CAM_CODEC" : "CC_VIS_CODEC")}};
    const auto list = [&](const QString& id) {
        QStringList items;
        for (const auto& item : value(id).toString().split(',', Qt::SkipEmptyParts)) {
            if (!item.trimmed().isEmpty())
                items.append(item.trimmed());
        }
        return items;
    };
    const QString prefix = camera ? "CC_CAM_OPT_" : "CC_VIS_OPT_";
    state["options"] = QVariantMap{{"resolution", list(prefix + "RES")},
                                   {"fps", list(prefix + "FPS")},
                                   {"bitrate", list(prefix + "BR")},
                                   {"codec", list(prefix + "CODEC")}};
    // Camera telemetry describes the active stream (Video or Vision); Vision telemetry only the inference.
    if (!fresh)
        state["current"] = QVariantMap();
    else if (camera)
        state["current"] = QVariantMap{{"name", _camName},
                                       {"connected", _cameraStatus == CC_CAM_STATUS_STREAMING},
                                       {"resolution", QString("%1x%2").arg(_videoWidth).arg(_videoHeight)},
                                       {"fps", double(_videoFps)},
                                       {"bitrate", _bitrateKbps},
                                       {"codec", _codec}};
    else
        state["current"] = QVariantMap{{"name", _modelName},
                                       {"connected", bool(_visionStatusFlags & 1)},
                                       {"resolution", QString("%1x%2").arg(_visionInputWidth).arg(_visionInputHeight)},
                                       {"fps", double(_inferenceFps)},
            {"confidence", double(_confidenceThresh)}};
    return state;
}

QVariantMap CompanionController::streamReason(const QString& stream) const
{
    const bool camera = stream == "camera";
    const QString label = camera ? QStringLiteral("Video") : QStringLiteral("Vision");
    const auto r = [](int code, const QString& text) { return QVariantMap{{"code", code}, {"text", text}}; };
    if (!companionState().value("connected").toBool()) return r(2, tr("Mất kết nối tới Companion"));
    if (!_toolbarReceived.contains(stream)) return r(3, tr("Chưa nhận telemetry %1 từ Companion. Node %1 có thể chưa chạy").arg(label));
    const qint64 age = _toolbarClock.elapsed() - _toolbarReceived.value(stream);
    if (age > 3500)
        return r(4, tr("Không nhận telemetry %1, lần cuối %2 s trước").arg(label).arg(age / 1000));
    const int version = _streamTelemetry.value(stream).value("version").toInt();
    if (version != 1) return r(5, tr("%1 trên Companion là bản cũ (config_version %2), cần cập nhật").arg(label).arg(version));
    if (_paramService->loadError()) return r(7, catalogError());
    if (_paramService->isLoading()) return r(6, tr("Đang tải tham số Companion…"));
    if (!_paramService->confirmedValue(camera ? "CC_CAM_RES" : "CC_VIS_RES").isValid()) return r(7, tr("%1 chưa trả tham số cấu hình").arg(label));
    // Camera and Vision are outside the arming lock (2026-10-13), so there is no "armed" reason (code 8) any more.
    if (_toolbarOperation && _toolbarOperation->origin != QLatin1String("stream"))
        return r(9, tr("Companion đang xử lý thao tác khác (%1)").arg(_toolbarOperation->group));
    return r(0, QString());
}

QVariantMap CompanionController::companionState() const
{
    const bool heartbeat = _toolbarReceived.contains("heartbeat") && _toolbarClock.elapsed() - _toolbarReceived.value("heartbeat") <= 3500;
    const bool fresh = _toolbarReceived.contains("system") && _toolbarClock.elapsed() - _toolbarReceived.value("system") <= 3500;
    const bool locked = configLocked();
    QMap<QString, QVariant> stage; // no request: each link's stage on the CC
    return {{"connected", heartbeat}, {"pending", bool(_toolbarOperation)}, {"fresh", fresh}, {"linkError", _toolbarErrors.value("links")}, {"name", fresh ? _companionName : QString()},
        {"cpuUsage", fresh ? QVariant(_cpuUsage) : QVariant()}, {"cpuTemp", fresh ? QVariant(_cpuTemp) : QVariant()},
        {"locked", locked}, {"lockReason", lockReason(locked)}, {"unsaved", _unsaved.value("links") && !_saveBlocked.value("links")},
        {"waitingReconnect", _toolbarOperation && _toolbarOperation->waitingReconnect},
        {"needsApply", applyPending("links", stage, {}).value_or(false)}, {"loadError", _paramService->loadError() ? catalogError() : QString()}};
}

QVariantMap CompanionController::hotspotState() const
{
    const bool fresh = _toolbarReceived.contains("network") && _toolbarClock.elapsed() - _toolbarReceived.value("network") <= 3500;
    QVariantMap state{{"available", fresh && _networkConfigVersion == 1}, {"fresh", fresh},
        {"wifiName", fresh ? _wlan0Ssid : QString()}, {"wifiConnected", fresh && _wlan0Status == CC_NET_STATUS_UP},
        {"ssid", fresh ? _apSsid : QString()}, {"open", fresh && _apStatus == CC_NET_STATUS_UP},
        {"ipCidr", fresh ? _apIp + "/" + QString::number(_apPrefixLength) : QString()},
        {"dhcpEnabled", fresh && _apDhcpEnabled}, {"pending", _toolbarOperation && _toolbarOperation->group == "hotspot"},
        {"error", _toolbarErrors.value("hotspot")}, {"operation", operationState("companion")},
        {"locked", configLocked()}, {"lockReason", lockReason(configLocked())}, {"unsaved", _unsaved.value("hotspot") && !_saveBlocked.value("hotspot")}};
    // The CC's stage as the request: an APPLY for it is still due when the AP shows another config (or SAVE is blocked).
    // Not read yet: an operation would only re-read it, so nothing is due.
    QMap<QString, QVariant> stage;
    for (const auto* id : {"CC_AP_SSID", "CC_AP_IP", "CC_AP_DHCP"}) stage[id] = _paramService->confirmedValue(id);
    const bool read = std::all_of(stage.cbegin(), stage.cend(), [](const QVariant& value) { return value.isValid(); });
    state["needsApply"] = read && applyPending("hotspot", stage, state).value_or(false);
    return state;
}

namespace {
// Same rule as the CC's SET check: rtsp only, and a path of plain segments (no %, no empty/./.. segment).
QString publishUrlError(const QString& input)
{
    const auto invalid = QStringLiteral("Địa chỉ RTSP không hợp lệ");
    if (input.toUtf8().size() > 127 || input.contains(QRegularExpression("[\\s\\x00-\\x1f\\x7f]")))
        return invalid;
    const QUrl url(input, QUrl::StrictMode);
    if (!(url.isValid() && url.scheme() == "rtsp" && !url.host().isEmpty() && !url.path().isEmpty() &&
          url.path() != "/" && url.userInfo().isEmpty() && url.fragment().isEmpty() && url.port(554) > 0 &&
          url.port(554) <= 65535))
        return invalid;
    static const QRegularExpression segments(QStringLiteral("^(/(?!\\.{1,2}(?=/|$))[A-Za-z0-9_.~-]+)+$"));
    if (input.left(input.indexOf('?') < 0 ? input.size() : input.indexOf('?')).contains('%') ||
        !segments.match(url.path()).hasMatch()) {
        return QStringLiteral(
            "Đường dẫn RTSP chỉ dùng A-Z a-z 0-9 _ . ~ / - (không có đoạn rỗng, . hoặc .., không có %)");
    }
    return QString();
}
}  // namespace

bool CompanionController::beginToolbarOperation(const QString& group, const QList<QPair<QString, QVariant>>& values,
                                                int subsystem, const QString& origin, const QStringList& skippedNotes)
{
    if (_paramService->loadError() || _paramService->isLoading() || _toolbarOperation || !companionState().value("connected").toBool() ||
        _sourceComponentId != kCompanionCompId || (_activeVehicle && _activeVehicle->id() != _sourceSystemId) ||
        (!streamGroup(group) && configLocked())) return false;
    ToolbarOperation op;
    op.group = group; op.subsystem = subsystem; op.epoch = _ccSessionEpoch; op.systemId = _sourceSystemId;
    for (const auto &entry : values) op.effectCandidate[entry.first] = entry.second;
    QStringList rideAlong;
    const auto pending = applyPending(group, op.effectCandidate, subsystem == 3 ? hotspotState() : QVariantMap(), &rideAlong);
    if (!pending) { _paramService->requestParameters(); return false; }
    for (const auto& name : rideAlong) op.steps.append({"info", tr("Kèm theo: %1 đang chờ áp dụng").arg(name), "done", 0});
    QMap<QString, QVariant> anticipated;
    for (const auto& entry : values) {
        auto confirmed = anticipated.contains(entry.first) ? anticipated.value(entry.first) : _paramService->confirmedValue(entry.first);
        if (!confirmed.isValid()) { _paramService->requestParameters(); return false; }
        const bool forced = entry.first == kModeId; // setStreamMode decides; a cancel resends the confirmed mode
        if (forced || confirmed.toString() != entry.second.toString()) {
            op.values.append(entry);
            op.previous[entry.first] = confirmed;
            anticipated[entry.first] = entry.second;
        }
    }
    if (op.values.isEmpty() && !*pending) return true; // nothing to set and nothing left to apply
    op.origin = origin.isEmpty() ? (streamGroup(group) ? QStringLiteral("stream") : QStringLiteral("companion")) : origin;
    op.startedMs = _toolbarClock.elapsed();
    for (const auto& note : skippedNotes)
        op.steps.append({"info", note, "skipped", 0});
    _toolbarErrors.remove(group);
    _toolbarOperation = op;
    if (!streamGroup(group))
        _toolbarOperationTimer->start(totalMs(group));
    publishOperation(QString());
    advanceToolbarOperation();
    return _toolbarOperation.has_value(); // false: its first SET could not be sent, the op already ended
}

std::optional<bool> CompanionController::applyPending(const QString& group, QMap<QString, QVariant>& candidate, const QVariantMap& ap,
                                                      QStringList* rideAlong) const
{
    // Still waiting to be applied: an APPLY of this group that was not confirmed, or a stage the CC runs differently.
    bool pending = _saveBlocked.value(group);
    if (group == QLatin1String("links")) {
        // APPLY applies every link value the CC has staged, so another link's stage that differs from its telemetry (a lost
        // APPLY, a half-sent stage) goes along and must be confirmed too. There is no reconcile step (spec §3).
        for (const auto& item : serialLinks()) {
            const auto link = item.toMap();
            const auto prefix = QString("CC_L%1_").arg(link.value("index").toInt());
            if (!_paramService->allFactNames().contains(prefix + "PORT")) continue; // the CC has no parameters for this link
            const bool edited = candidate.contains(prefix + "PORT"); // what this operation asks for, set or staged
            bool differs = false;
            for (const auto &field : {QPair<QString, QString>{"BAUD", "baudrate"}, {"PORT", "port"}}) {
                const auto configured = edited ? candidate.value(prefix + field.first) : _paramService->confirmedValue(prefix + field.first);
                if (!configured.isValid()) return std::nullopt; // not read yet
                // shortcut: a link without fresh telemetry cannot be compared and is left out; upgrade if the CC stops reporting one
                if (link.value("fresh").toBool() && configured.toString() != link.value(field.second).toString()) {
                    candidate[prefix + field.first] = configured;
                    differs = true;
                }
            }
            if (differs && !edited && rideAlong) rideAlong->append(link.value("name").toString());
            pending = pending || differs;
        }
    } else if (group == QLatin1String("hotspot")) {
        pending = pending || (ap.value("fresh").toBool() && !apShows(ap, candidate));
    }
    return pending;
}

void CompanionController::advanceToolbarOperation()
{
    if (!_toolbarOperation)
        return;
    auto &op = *_toolbarOperation;
    if (op.epoch != _ccSessionEpoch) { finishToolbarOperation(tr("Mất kết nối Companion")); return; }
    if (op.next < op.values.size()) {
        const auto entry = op.values[op.next++];
        op.awaiting = entry.first;
        op.sawInProgress = false;
        op.inProgressAcks = 0;
        op.lastText.clear(); // the CC's refusal reason for this SET comes after it
        const bool secret = _paramService->isSecret(entry.first);
        addOperationStep("tx",
                         QStringLiteral("%1PARAM_EXT_SET %2=%3")
                             .arg(op.rollingBack ? QStringLiteral("(rollback) ") : QString(), entry.first,
                                  secret ? QStringLiteral("<hidden>") : entry.second.toString()),
                         "waiting");
        _setAckTimer->start(streamGroup(op.group) ? silenceMs() : firstAckMs());
        if (!_paramService->sendSingleParamSet(entry.first, entry.second)) finishToolbarOperation(tr("Không gửi được tới Companion"));
        return;
    }
    op.awaiting.clear();
    _setAckTimer->stop();
    if (op.rollingBack) {
        finishToolbarOperation(op.rollbackError + "; previous stage restored");
        return;
    }
    if (op.subsystem) {
        op.waitingCommand = true;
        op.applySent = true;
        op.command = MAV_CMD_THACO_APPLY_CONFIG;
        op.effectAfter = _toolbarClock.elapsed();
        addOperationStep("tx", QStringLiteral("APPLY %1").arg(op.group), "waiting");
        if (!sendCompanionCommand(MAV_CMD_THACO_APPLY_CONFIG, op.subsystem, 1)) {
            finishToolbarOperation(tr("Không gửi được tới Companion"));
            return;
        }
        _setAckTimer->start(silenceMs()); // 3 s of CC silence ends the wait (spec §5)
    } else finishToolbarOperation();
}

void CompanionController::toolbarParameterSaved(const QString& name, bool success)
{
    if (!_toolbarOperation || _toolbarOperation->awaiting != name)
        return;
    auto &op = *_toolbarOperation;
    if (op.expectCancelAck) {
        // The final ACK of the mode operation this one replaced; it carries no news about this one.
        op.expectCancelAck = false;
        addOperationStep("rx", tr("ACK của thao tác đã hủy"), "skipped");
        return;
    }
    _setAckTimer->stop();
    const auto expected = op.values[op.next - 1].second;
    // A secret is never echoed back, so only the result code counts for it.
    const bool matches =
        _paramService->isSecret(name) || _paramService->confirmedValue(name).toString() == expected.toString();
    if (!success || !matches) {
        settleWaitingSteps("error");
        addOperationStep("rx", success ? QStringLiteral("ACK khác giá trị đã gửi") : QStringLiteral("ACK FAILED"), "error");
        // The CC names the reason in a "<Group>: " line right before its FAILED.
        const QString reason = !op.lastText.isEmpty() ? op.lastText : success ? tr("ACK khác giá trị đã gửi") : tr("CC từ chối (không có lý do)");
        // Cancel only this session's successful stage writes, in ACK order (nothing is applied yet).
        if (op.subsystem && !op.waitingCommand && !op.rollingBack && !configLocked()) {
            QList<QPair<QString, QVariant>> rollback;
            for (int i = op.next - 2; i >= 0; --i)
                rollback.append({op.values[i].first, op.previous.value(op.values[i].first)});
            if (!rollback.isEmpty()) {
                op.values = rollback; op.next = 0; op.rollingBack = true; op.rollbackError = reason;
                advanceToolbarOperation(); return;
            }
        }
        finishToolbarOperation(reason);
        return;
    }
    settleWaitingSteps("done");
    addOperationStep("rx", QStringLiteral("ACK ACCEPTED"), "done");
    if (!op.rollingBack)
        op.progress = qMin(1.0, op.progress + (op.subsystem ? 0.5 : 1.0) / op.values.size());
    advanceToolbarOperation();
}

void CompanionController::syncPlayerToMode()
{
    // Any source moves the confirmed mode (own op, late ACCEPTED, readback); reconnect the player once per new mode
    // instead of waiting for it to notice the old stream is gone. The first known mode only records the start state.
    const QVariant confirmed = _paramService->confirmedValue(kModeId);
    if (!confirmed.isValid() || confirmed.toInt() == _playerMode)
        return;
    const bool first = _playerMode < 0;
    _playerMode = confirmed.toInt();
    if (first)
        return;
    VideoManager::instance()->stopVideo();
    if (_playerMode != 0)
        VideoManager::instance()->startVideo();
    emit videoRestarted(_playerMode);
}

void CompanionController::finishToolbarOperation(const QString& error)
{
    if (!_toolbarOperation)
        return;
    auto &op = *_toolbarOperation;
    const auto group = op.group;
    _setAckTimer->stop();
    _toolbarOperationTimer->stop();
    settleWaitingSteps(error.isEmpty() ? "done" : "error");
    // Links/hotspot are not autosaved any more (camera/Vision are): SAVE only after a confirmed APPLY (spec §7).
    const bool applied = op.command == MAV_CMD_THACO_APPLY_CONFIG;
    recordApplyOutcome(error.isEmpty());
    if (error.isEmpty()) {
        addOperationStep("info", applied ? tr("Đã áp dụng, chưa lưu — reboot sẽ về cấu hình cũ") : tr("Đã lưu"), "done");
        op.progress = 1.0;
    } else {
        addOperationStep("info", error, "error");
    }
    publishOperation(error.isEmpty() ? QStringLiteral("confirmed") : QStringLiteral("error"), error);
    _toolbarOperation.reset();
    _toolbarErrors[group] = error;
    emit toolbarStateChanged();
    if (!error.isEmpty()) _paramService->requestParameters();
}

QVariantMap CompanionController::operationState(const QString& origin) const
{
    return _lastOperation.value(origin, QVariantMap{{"active", false},
                                                    {"progress", 0.0},
                                                    {"steps", QVariantList()},
                                                    {"error", QString()},
                                                    {"outcome", QString()}});
}

void CompanionController::addOperationStep(const QString& direction, const QString& text, const QString& state)
{
    if (!_toolbarOperation)
        return;
    auto& op = *_toolbarOperation;
    op.steps.append({direction, text, state, _toolbarClock.elapsed() - op.startedMs});
    while (op.steps.size() > 30)
        op.steps.removeFirst();
    publishOperation(QString());
}

void CompanionController::settleWaitingSteps(const QString& state)
{
    if (!_toolbarOperation)
        return;
    for (auto& step : _toolbarOperation->steps) {
        if (step.state == QLatin1String("waiting"))
            step.state = state;
    }
}

void CompanionController::publishOperation(const QString& outcome, const QString& error)
{
    if (!_toolbarOperation)
        return;
    const auto& op = *_toolbarOperation;
    QVariantList steps;
    for (const auto& step : op.steps) {
        steps.append(QVariantMap{
            {"direction", step.direction}, {"text", step.text}, {"state", step.state}, {"elapsedMs", step.elapsedMs}});
    }
    _lastOperation[op.origin] = QVariantMap{{"active", outcome.isEmpty()},
                                            {"progress", op.progress},
                                            {"steps", steps},
                                            {"error", error},
                                            {"outcome", outcome}};
    emit toolbarStateChanged();
}

void CompanionController::onApplyProgress(uint8_t progress)
{
    if (!_toolbarOperation || !_toolbarOperation->applySent || _toolbarOperation->resultSeen)
        return;
    auto& op = *_toolbarOperation;
    if (progress == op.lastApplyProgress)
        return;  // the CC repeats the last stage every 500 ms to keep the command alive
    op.lastApplyProgress = progress;
    op.progress = qMax(op.progress, 0.5 + progress / 200.0);
    settleWaitingSteps("done");
    addOperationStep("rx", stageText(op.subsystem, progress), "done");
}

void CompanionController::onApplyResult(uint8_t result, bool noResponse)
{
    if (!_toolbarOperation || !_toolbarOperation->applySent || _toolbarOperation->resultSeen)
        return;
    auto& op = *_toolbarOperation;
    if (noResponse) {
        // The CC's first reply to an APPLY is IN_PROGRESS, so with one it has the command and its silence is the router/AP
        // restart dropping the link (spec §5): the change may well have worked. Without one the APPLY may never have arrived.
        if (op.command == MAV_CMD_THACO_APPLY_CONFIG && op.lastApplyProgress >= 0) { enterTelemetryWait(); return; }
        op.resultSeen = true;
        finishToolbarOperation(tr("Companion không phản hồi"));
        return;
    }
    op.resultSeen = true;
    if (result != MAV_RESULT_ACCEPTED) {
        op.refused = true;
        settleWaitingSteps("error");
        QString reason = op.lastText;
        if (reason.isEmpty()) {
            reason = (result == MAV_RESULT_DENIED && op.group == "hotspot") ? tr("Mật khẩu hiện tại không đúng")
                   : result == MAV_RESULT_TEMPORARILY_REJECTED ? tr("Companion từ chối tạm thời (đang arm, bận hoặc router chưa sẵn sàng)")
                   : op.command == MAV_CMD_THACO_SAVE_DEFAULT_CONFIG ? tr("Lưu bị từ chối (%1)").arg(result)
                   : tr("Apply bị từ chối (%1)").arg(result);
        }
        finishToolbarOperation(reason);
        return;
    }
    settleWaitingSteps("done");
    addOperationStep("rx", QStringLiteral("ACK ACCEPTED"), "done");
    if (op.command == MAV_CMD_THACO_SAVE_DEFAULT_CONFIG) {
        _unsaved[op.group] = false;
        finishToolbarOperation();
        return;
    }
    op.waitingEffect = true;
    checkToolbarEffect();
}

void CompanionController::enterTelemetryWait()
{
    if (!_toolbarOperation)
        return;
    auto& op = *_toolbarOperation;
    op.resultSeen = true;
    op.waitingEffect = true;
    op.unconfirmedOnTimeout = true;
    settleWaitingSteps("done");
    addOperationStep("info",
                     op.subsystem == 3
        ? tr("Đã gửi cấu hình hotspot mới. Mất kết nối WiFi là bình thường; hãy nối lại vào SSID mới")
                         : tr("Router đang khởi động lại, chờ telemetry"),
                     "waiting");
    checkToolbarEffect();
}

void CompanionController::setAckTimedOut()
{
    if (_toolbarOperation && _toolbarOperation->applySent && !_toolbarOperation->resultSeen) { onApplyResult(0, true); return; }
    if (!_toolbarOperation || _toolbarOperation->awaiting.isEmpty()) return;
    // Silence is a lost link or a dead CC, not a failed change: the CC may still finish it.
    if (streamGroup(_toolbarOperation->group)) {
        endUnconfirmed(tr("CC không phản hồi"));
        return;
    }
    failOperationByTimeout(_toolbarOperation->sawInProgress
                               ? tr("Không có kết quả sau %1 s").arg(finalAckMs() / 1000.0, 0, 'g', 2)
                                                           : tr("Companion không phản hồi"));
}

bool CompanionController::setStreamParameter(const QString& stream, const QString& id, const QVariant& value)
{
    const bool camera = stream == "camera";
    if (!camera && stream != "vision")
        return false;
    const QStringList allowed =
        camera ? QStringList{"CC_CAM_RES", "CC_CAM_FPS", "CC_CAM_BR", "CC_CAM_CODEC", "CC_CAM_PUB_URL"}
        : QStringList{"CC_VIS_RES", "CC_VIS_FPS", "CC_VIS_BR", "CC_VIS_CODEC", "CC_VIS_OUT_URL", "CC_VIS_CONF"};
    // camera_streamer owns the Vision video parameters too; only the inference ones need the Vision node.
    const bool visionNode = id == "CC_VIS_FPS" || id == "CC_VIS_CONF";
    if (!allowed.contains(id) || !streamState(visionNode ? "vision" : "camera").value("available").toBool())
        return false;
    if (id.endsWith("_URL")) {
        const QString urlError = publishUrlError(value.toString());
        if (!urlError.isEmpty())
            return refuseStream(urlError);
    }
    if (id.endsWith("CODEC")) {
        auto mask = _streamTelemetry.value("camera").value("supportedCodecs").toInt();
        if ((value.toString() == "h264" && !(mask & 1)) || (value.toString() == "h265" && !(mask & 2)) ||
            (value.toString() != "h264" && value.toString() != "h265"))
            return false;
    }
    return beginToolbarOperation(stream, {{id, value}});
}

bool CompanionController::modeOperationPending() const
{
    return _toolbarOperation && _toolbarOperation->previous.contains(kModeId);
}

bool CompanionController::refuseStream(const QString& why)
{
    // Shown in the stream log: added to the running op, or as a refused one when nothing runs.
    if (_toolbarOperation && _toolbarOperation->origin == QLatin1String("stream")) {
        addOperationStep("info", why, "error");
        return false;
    }
    _lastOperation["stream"] = QVariantMap{
        {"active", false},
        {"progress", 0.0},
        {"outcome", QStringLiteral("error")},
        {"error", why},
        {"steps",
         QVariantList{QVariantMap{{"direction", "info"}, {"text", why}, {"state", "error"}, {"elapsedMs", 0}}}}};
    emit toolbarStateChanged();
    return false;
}

QString CompanionController::modeBlockedReason() const
{
    const auto reason = streamReason("camera");
    if (reason.value("code").toInt() != 0)
        return reason.value("text").toString();
    // CC_VIS_CONF/FPS run in the CC's Vision slot, which a mode SET does not cancel, and QGC tracks one op at a time.
    if (_toolbarOperation && _toolbarOperation->group == QLatin1String("vision")) {
        const QString id = _toolbarOperation->values.first().first;
        if (id == QLatin1String("CC_VIS_CONF") || id == QLatin1String("CC_VIS_FPS"))
            return tr("Đang chờ Vision áp dụng %1; đổi chế độ khi xong").arg(id);
    }
    return QString();
}

QVariantMap CompanionController::streamModeState() const
{
    const bool modeOp = modeOperationPending();
    // While a mode operation runs, the confirmed mode is the one before it (a replaced operation never confirmed its
    // own).
    const QVariant confirmed =
        modeOp ? _toolbarOperation->previous.value(kModeId) : _paramService->confirmedValue(kModeId);
    const int mode = confirmed.isValid() ? confirmed.toInt() : -1;
    const bool visionFresh =
        _toolbarReceived.contains("vision") && _toolbarClock.elapsed() - _toolbarReceived.value("vision") <= 3500;
    return {{"mode", mode},
            {"pendingMode", modeOp ? _toolbarOperation->values.first().second.toInt() : mode},
            {"modeOperation", modeOp},
            {"pending", _toolbarOperation && streamGroup(_toolbarOperation->group)},
            {"operation", operationState(QStringLiteral("stream"))},
            {"reason", streamReason("camera")},
            {"visionFresh", visionFresh},
            {"blocked", modeBlockedReason()}};
}

bool CompanionController::setStreamMode(int mode)
{
    if (mode < 0 || mode > 2)
        return false;
    const QString blocked = modeBlockedReason();
    if (!blocked.isEmpty())
        return refuseStream(blocked);
    const QString notSent = tr("Chưa gửi được yêu cầu đổi chế độ, thử lại");
    if (!_toolbarOperation) {
        const auto confirmed = _paramService->confirmedValue(kModeId);
        if (confirmed.isValid() && confirmed.toInt() == mode)
            return true;
        return beginToolbarOperation("camera", {{kModeId, mode}}) || refuseStream(notSent);
    }
    // A camera-group op is pending: a mode op to replace, or a settings op (maybe a stuck rebuild) to drop. The CC ends
    // either one on a CC_STREAM_MODE SET.
    const ToolbarOperation replaced = *_toolbarOperation;
    const bool modeOp = replaced.previous.contains(kModeId);
    if (modeOp && replaced.values.first().second.toInt() == mode)
        return true;
    _toolbarOperation.reset();
    if (!beginToolbarOperation("camera", {{kModeId, mode}})) {
        _toolbarOperation = replaced; // nothing was sent: the old op goes on
        _setAckTimer->start(silenceMs()); // a replacing SET that failed to send stopped the old op's silence timer
        return refuseStream(notSent);
    }
    auto& op = *_toolbarOperation;
    if (modeOp) {
        // The CC answers the replaced mode op (same parameter) before the new one's first IN_PROGRESS.
        op.previous[kModeId] = replaced.previous.value(kModeId);
        op.expectCancelAck = true;
    } // A dropped settings op's FAILED names another parameter, so `awaiting` already filters it.
    auto steps = replaced.steps;
    for (auto& step : steps)
        if (step.state == QLatin1String("waiting"))
            step.state = QStringLiteral("skipped");
    steps.append({"info",
                  modeOp ? tr("Đã hủy chuyển sang %1").arg(modeName(replaced.values.first().second.toInt()))
                         : tr("Bỏ thao tác đang chờ để đổi chế độ"),
                  "skipped", 0});
    op.steps = steps + op.steps;
    while (op.steps.size() > 30)
        op.steps.removeFirst();
    publishOperation(QString());
    return true;
}

void CompanionController::cancelStreamOperation()
{
    if (modeOperationPending())
        setStreamMode(_toolbarOperation->previous.value(kModeId).toInt());
}

bool CompanionController::setLinkConfig(int index, const QString& port, int baud)
{
    QVariantMap self, other;
    for (const auto& v : serialLinks()) {
        const auto link = v.toMap();
        if (link.value("index").toInt() == index) self = link;
        else if (!port.isEmpty() && link.value("port").toString() == port) other = link; // "" = off, shared by any number
    }
    if (!self.value("fresh").toBool()) return false; // also an index the CC does not report
    if (!other.isEmpty()) {
        _toolbarErrors["links"] = tr("Cổng đang được dùng bởi %1").arg(other.value("name").toString());
        emit toolbarStateChanged();
        return false;
    }
    return beginToolbarOperation("links",
                                 {{QString("CC_L%1_PORT").arg(index), port}, {QString("CC_L%1_BAUD").arg(index), baud}},
                                 1, "companion");
}

bool CompanionController::applyHotspot(const QVariantMap& draft)
{
    if (!hotspotState().value("available").toBool())
        return false;
    QList<QPair<QString, QVariant>> values{{"CC_AP_SSID", draft.value("ssid")},
                                           {"CC_AP_IP", draft.value("ipCidr")},
                                           {"CC_AP_DHCP", draft.value("dhcpEnabled").toBool() ? 1 : 0}};
    if (!draft.value("password").toString().isEmpty())
        values.append({"CC_AP_PASS", draft.value("password")});
    return beginToolbarOperation("hotspot", values, 3);
}

void CompanionController::checkToolbarEffect()
{
    if (!_toolbarOperation || !_toolbarOperation->waitingEffect) return;
    const auto &op = *_toolbarOperation;
    if (op.subsystem == 1) {
        for (auto it = op.effectCandidate.cbegin(); it != op.effectCandidate.cend(); ++it) {
            const auto match = QRegularExpression("^CC_L([0-7])_(BAUD|PORT)$").match(it.key());
            if (!match.hasMatch())
                return;
            const int index = match.captured(1).toInt();
            const QString field = match.captured(2) == "BAUD" ? QStringLiteral("baudrate") : QStringLiteral("port");
            bool confirmed = false;
            for (const auto &v : serialLinks()) {
                const auto link = v.toMap();
                if (link.value("index").toInt() == index && link.value("fresh").toBool() &&
                    _toolbarReceived.value(QString("link:%1").arg(index), -1) > op.effectAfter &&
                    link.value(field).toString() == it.value().toString())
                    confirmed = true;
            }
            if (!confirmed)
                return;
        }
    } else if (op.subsystem == 3) {
        const auto active = hotspotState();
        if (!active.value("available").toBool() || !active.value("open").toBool() || _toolbarReceived.value("network", -1) <= op.effectAfter) return;
        if (!apShows(active, op.effectCandidate)) {
            // Back after the restart with the AP up on another config: the candidate did not come up.
            if (op.readbackRequested) finishToolbarOperation(tr("Không áp dụng được"));
            return;
        }
    } else
        return;
    addOperationStep("rx", tr("Telemetry xác nhận cấu hình mới"), "done");
    finishToolbarOperation();
}

void CompanionController::toolbarTimedOut()
{
    if (!_toolbarOperation)
        return;
    auto &op = *_toolbarOperation;
    // Only a hotspot's window after the CC came back runs while waiting for a reconnect (onCompanionSessionStarted).
    if (op.waitingReconnect) { endUnconfirmed(tr("Chưa xác nhận: AP chưa phát cấu hình mới")); return; }
    if (op.unconfirmedOnTimeout) {
        // Telemetry never confirmed it, but nothing failed either: leave the steps for the user to read.
        endUnconfirmed(QString());
        return;
    }
    failOperationByTimeout(tr("Hết thời gian chờ, chưa rõ kết quả"));
}

void CompanionController::endUnconfirmed(const QString& note)
{
    if (!_toolbarOperation)
        return;
    const auto group = _toolbarOperation->group;
    recordApplyOutcome(false);
    _setAckTimer->stop();
    _toolbarOperationTimer->stop();
    if (!note.isEmpty()) {
        settleWaitingSteps("skipped");
        addOperationStep("info", note, "error");
    }
    publishOperation(QStringLiteral("unconfirmed"));
    _toolbarOperation.reset();
    _toolbarErrors[group].clear();
    emit toolbarStateChanged();
}

void CompanionController::recordApplyOutcome(bool confirmed)
{
    const auto& op = *_toolbarOperation;
    // The outcome belongs to the CC the APPLY went to, also while the lock is reset (_sourceSystemId -1, the flags kept for
    // that CC's return); another CC locked since has its own flags (cleared when it locked).
    if (op.command != MAV_CMD_THACO_APPLY_CONFIG || op.systemId != _lastCompanionSystemId) return;
    if (confirmed) { _unsaved[op.group] = true; _saveBlocked.remove(op.group); }
    // No rollback: an unconfirmed, cancelled or unanswered candidate may be what the CC runs now, and SAVE would persist it.
    // A hotspot candidate stays active after a refusal too. A refused links APPLY leaves the active links as they were,
    // so what was applied before can still be saved.
    else if (op.group == QLatin1String("hotspot") || !op.refused) _saveBlocked[op.group] = true;
}

void CompanionController::failOperationByTimeout(const QString& reason)
{
    if (!_toolbarOperation)
        return;
    auto &op = *_toolbarOperation;
    _setAckTimer->stop();
    settleWaitingSteps("error");
    if (op.subsystem && !op.waitingCommand && !op.rollingBack && !configLocked()) {
        QList<QPair<QString, QVariant>> rollback;
        // Include the last sent stage: its effect is uncertain when its ACK was lost.
        for (int i = op.next - 1; i >= 0; --i)
            rollback.append({op.values[i].first, op.previous.value(op.values[i].first)});
        if (!rollback.isEmpty()) {
            addOperationStep("info", reason, "error");
            op.values = rollback;
            op.next = 0;
            op.rollingBack = true;
            op.rollbackError = "Stage timed out";
            _toolbarOperationTimer->start(firstAckMs() + finalAckMs());
            advanceToolbarOperation();
            return;
        }
    }
    finishToolbarOperation(reason + QStringLiteral("; effect unknown, refresh readback before retrying"));
}

bool CompanionController::logMatchesFilter(const QString& prefix, const QString& category, const QString& text) const
{
    return CompanionLogService::matchesFilter(prefix, category, text);
}
