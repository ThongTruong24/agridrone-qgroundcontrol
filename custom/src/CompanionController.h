#pragma once

#include <QtCore/QMap>
#include <QtCore/QSet>

#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>
#include <optional>
#include <QtCore/QString>
#include <QtCore/QPointer>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtCore/QTimer>
#include <QtQmlIntegration/QtQmlIntegration>
#include <functional>
#include "LinkInterface.h"
#include "QGCMAVLink.h"
#include "Vehicle.h"

#include "CompanionLinksService.h"
#include "CompanionParamService.h"
#include "CompanionMavlinkDispatcher.h"

/**
 * @brief Facade controller exposing Companion Computer features to QML (SOLID: Facade pattern)
 */
class CompanionController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap cameraStreamState READ cameraStreamState NOTIFY toolbarStateChanged)
    Q_PROPERTY(QVariantMap visionStreamState READ visionStreamState NOTIFY toolbarStateChanged)
    Q_PROPERTY(QVariantMap streamModeState READ streamModeState NOTIFY toolbarStateChanged)
    Q_PROPERTY(QVariantMap hotspotState READ hotspotState NOTIFY toolbarStateChanged)
    Q_PROPERTY(QVariantList serialLinks READ serialLinks NOTIFY toolbarStateChanged)
    Q_PROPERTY(QVariantMap companionState READ companionState NOTIFY toolbarStateChanged)

    QML_ELEMENT
    QML_SINGLETON
    friend class CompanionControllerTest;
    friend class CompanionVehicleLifecycleTest;

    // Vehicle status
    Q_PROPERTY(bool vehicleConnected READ vehicleConnected NOTIFY vehicleConnectedChanged)

    // Extended Parameters (CompID 191)
    Q_PROPERTY(bool ccParametersLoading READ ccParametersLoading NOTIFY ccParametersLoadingChanged)

    // Telemetry Received Flags
    Q_PROPERTY(bool hasCameraTelemetry READ hasCameraTelemetry NOTIFY cameraChanged)
    Q_PROPERTY(bool hasVisionTelemetry READ hasVisionTelemetry NOTIFY visionChanged)
    Q_PROPERTY(bool hasNetworkTelemetry READ hasNetworkTelemetry NOTIFY networkChanged)
    Q_PROPERTY(bool hasLinksTelemetry READ hasLinksTelemetry NOTIFY linksChanged)
    Q_PROPERTY(bool hasSystemTelemetry READ hasSystemTelemetry NOTIFY systemChanged)

    // Camera Properties (from CC_TELEMETRY_CAMERA 42011)
    Q_PROPERTY(int camStatus READ camStatus NOTIFY cameraChanged)
    Q_PROPERTY(QString camName READ camName NOTIFY cameraChanged)
    Q_PROPERTY(QString cameraType READ cameraType NOTIFY cameraChanged)
    Q_PROPERTY(QString connectionPort READ connectionPort NOTIFY cameraChanged)
    Q_PROPERTY(QString serialNumber READ serialNumber NOTIFY cameraChanged)
    Q_PROPERTY(int videoWidth READ videoWidth NOTIFY cameraChanged)
    Q_PROPERTY(int videoHeight READ videoHeight NOTIFY cameraChanged)
    Q_PROPERTY(int videoFps READ videoFps NOTIFY cameraChanged)
    Q_PROPERTY(int depthWidth READ depthWidth NOTIFY cameraChanged)
    Q_PROPERTY(int depthHeight READ depthHeight NOTIFY cameraChanged)
    Q_PROPERTY(int depthFps READ depthFps NOTIFY cameraChanged)
    Q_PROPERTY(int bitrateKbps READ bitrateKbps NOTIFY cameraChanged)
    Q_PROPERTY(int bitrateMaxKbps READ bitrateMaxKbps NOTIFY cameraChanged)
    Q_PROPERTY(int vbvBufferKb READ vbvBufferKb NOTIFY cameraChanged)
    Q_PROPERTY(int rotation READ rotation NOTIFY cameraChanged)
    Q_PROPERTY(int rtspPort READ rtspPort NOTIFY cameraChanged)
    Q_PROPERTY(QString rtspUrlQgc READ rtspUrlQgc NOTIFY cameraChanged)
    Q_PROPERTY(QString rtspUrlController READ rtspUrlController NOTIFY cameraChanged)
    Q_PROPERTY(QString rtspUrlLaptop READ rtspUrlLaptop NOTIFY cameraChanged)
    Q_PROPERTY(QString codec READ codec NOTIFY cameraChanged)
    Q_PROPERTY(QString encoderMode READ encoderMode NOTIFY cameraChanged)
    Q_PROPERTY(bool isDefaultCam READ isDefaultCam NOTIFY cameraChanged)
    Q_PROPERTY(int profileMode READ profileMode NOTIFY cameraChanged)
    Q_PROPERTY(int enableEmitter READ enableEmitter NOTIFY cameraChanged)
    Q_PROPERTY(int cameraStatus READ cameraStatus NOTIFY cameraChanged)
    Q_PROPERTY(int errorCode READ errorCode NOTIFY cameraChanged)
    Q_PROPERTY(int usbSpeedMode READ usbSpeedMode NOTIFY cameraChanged)

    // Vision Properties (from CC_TELEMETRY_VISION 42013)
    Q_PROPERTY(float confidenceThresh READ confidenceThresh NOTIFY visionChanged)
    Q_PROPERTY(float inferenceFps READ inferenceFps NOTIFY visionChanged)
    Q_PROPERTY(int visionInputWidth READ visionInputWidth NOTIFY visionChanged)
    Q_PROPERTY(int visionInputHeight READ visionInputHeight NOTIFY visionChanged)
    Q_PROPERTY(int visionVideoFps READ visionVideoFps NOTIFY visionChanged)
    Q_PROPERTY(int detectionsCount READ detectionsCount NOTIFY visionChanged)
    Q_PROPERTY(int visionStatusFlags READ visionStatusFlags NOTIFY visionChanged)
    Q_PROPERTY(QString modelName READ modelName NOTIFY visionChanged)
    Q_PROPERTY(QString inputSource READ inputSource NOTIFY visionChanged)

    // Network Properties (from CC_TELEMETRY_NETWORK 42012)
    Q_PROPERTY(QString eth0Ip READ eth0Ip NOTIFY networkChanged)
    Q_PROPERTY(QString eth0Netmask READ eth0Netmask NOTIFY networkChanged)
    Q_PROPERTY(int eth0Status READ eth0Status NOTIFY networkChanged)
    Q_PROPERTY(int eth0IsStatic READ eth0IsStatic NOTIFY networkChanged)
    Q_PROPERTY(quint32 eth0RxKb READ eth0RxKb NOTIFY networkChanged)
    Q_PROPERTY(quint32 eth0TxKb READ eth0TxKb NOTIFY networkChanged)
    Q_PROPERTY(QString wlan0Ip READ wlan0Ip NOTIFY networkChanged)
    Q_PROPERTY(QString wlan0Netmask READ wlan0Netmask NOTIFY networkChanged)
    Q_PROPERTY(int wlan0Status READ wlan0Status NOTIFY networkChanged)
    Q_PROPERTY(QString wlan0Ssid READ wlan0Ssid NOTIFY networkChanged)
    Q_PROPERTY(int wlan0Rssi READ wlan0Rssi NOTIFY networkChanged)
    Q_PROPERTY(int wlan0Dhcp READ wlan0Dhcp NOTIFY networkChanged)
    Q_PROPERTY(quint32 wlan0RxKb READ wlan0RxKb NOTIFY networkChanged)
    Q_PROPERTY(quint32 wlan0TxKb READ wlan0TxKb NOTIFY networkChanged)
    Q_PROPERTY(QString apIp READ apIp NOTIFY networkChanged)
    Q_PROPERTY(QString apNetmask READ apNetmask NOTIFY networkChanged)
    Q_PROPERTY(QString apSsid READ apSsid NOTIFY networkChanged)
    Q_PROPERTY(int apChannel READ apChannel NOTIFY networkChanged)
    Q_PROPERTY(int apClientCount READ apClientCount NOTIFY networkChanged)
    Q_PROPERTY(int apStatus READ apStatus NOTIFY networkChanged)
    Q_PROPERTY(QString apHwMode READ apHwMode NOTIFY networkChanged)
    Q_PROPERTY(quint32 uap0RxKb READ uap0RxKb NOTIFY networkChanged)
    Q_PROPERTY(quint32 uap0TxKb READ uap0TxKb NOTIFY networkChanged)
    Q_PROPERTY(int dnsmasqStatus READ dnsmasqStatus NOTIFY networkChanged)

    // Links Properties (delegated to CompanionLinksService)
    Q_PROPERTY(int fcBaud READ fcBaud NOTIFY linksChanged)
    Q_PROPERTY(int siyiBaud READ siyiBaud NOTIFY linksChanged)
    Q_PROPERTY(QString fcPort READ fcPort NOTIFY linksChanged)
    Q_PROPERTY(QString siyiPort READ siyiPort NOTIFY linksChanged)
    Q_PROPERTY(int fcStatus READ fcStatus NOTIFY linksChanged)
    Q_PROPERTY(int siyiStatus READ siyiStatus NOTIFY linksChanged)
    Q_PROPERTY(float fcBitrateKbps READ fcBitrateKbps NOTIFY linksChanged)
    Q_PROPERTY(float fcPacketDropRate READ fcPacketDropRate NOTIFY linksChanged)
    Q_PROPERTY(float siyiBitrateKbps READ siyiBitrateKbps NOTIFY linksChanged)
    Q_PROPERTY(float siyiPacketDropRate READ siyiPacketDropRate NOTIFY linksChanged)
    Q_PROPERTY(quint32 fcBytesRx READ fcBytesRx NOTIFY linksChanged)
    Q_PROPERTY(quint32 fcBytesTx READ fcBytesTx NOTIFY linksChanged)
    Q_PROPERTY(quint32 siyiBytesRx READ siyiBytesRx NOTIFY linksChanged)
    Q_PROPERTY(quint32 siyiBytesTx READ siyiBytesTx NOTIFY linksChanged)
    Q_PROPERTY(int siyiLinkQuality READ siyiLinkQuality NOTIFY linksChanged)
    Q_PROPERTY(int linkStatusFlags READ linkStatusFlags NOTIFY linksChanged)

    // Channel Rates & Stats for FC
    Q_PROPERTY(float fcTxRate READ fcTxRate NOTIFY linksChanged)
    Q_PROPERTY(float fcRxRate READ fcRxRate NOTIFY linksChanged)
    Q_PROPERTY(float fcTxRateMax READ fcTxRateMax NOTIFY linksChanged)
    Q_PROPERTY(float fcTxRateMulti READ fcTxRateMulti NOTIFY linksChanged)
    Q_PROPERTY(float fcRxLoss READ fcRxLoss NOTIFY linksChanged)
    Q_PROPERTY(quint32 fcTxErr READ fcTxErr NOTIFY linksChanged)

    // Channel Rates & Stats for SIYI
    Q_PROPERTY(float siyiTxRate READ siyiTxRate NOTIFY linksChanged)
    Q_PROPERTY(float siyiRxRate READ siyiRxRate NOTIFY linksChanged)
    Q_PROPERTY(float siyiTxRateMax READ siyiTxRateMax NOTIFY linksChanged)
    Q_PROPERTY(float siyiTxRateMulti READ siyiTxRateMulti NOTIFY linksChanged)
    Q_PROPERTY(float siyiRxLoss READ siyiRxLoss NOTIFY linksChanged)
    Q_PROPERTY(quint32 siyiTxErr READ siyiTxErr NOTIFY linksChanged)

    // Transport Protocol & Dynamic Ports from CC
    Q_PROPERTY(QString transportProtocol READ transportProtocol NOTIFY linksChanged)
    Q_PROPERTY(QStringList availablePorts READ availablePorts NOTIFY availablePortsChanged)
    Q_PROPERTY(bool vehicleAvailable READ vehicleAvailable NOTIFY vehicleAvailableChanged)

    // System Properties (from CC_TELEMETRY_SYSTEM 42014)
    Q_PROPERTY(int cpuUsage READ cpuUsage NOTIFY systemChanged)
    Q_PROPERTY(int ramUsage READ ramUsage NOTIFY systemChanged)
    Q_PROPERTY(int diskUsage READ diskUsage NOTIFY systemChanged)
    Q_PROPERTY(int cpuTemp READ cpuTemp NOTIFY systemChanged)
    Q_PROPERTY(quint32 uptimeS READ uptimeS NOTIFY systemChanged)

    // Toast / Feedback
    Q_PROPERTY(QString lastToastMsg READ lastToastMsg NOTIFY toastChanged)
    Q_PROPERTY(bool lastToastIsError READ lastToastIsError NOTIFY toastChanged)

public:
    explicit CompanionController(QObject* parent = nullptr);
    ~CompanionController() override;

    bool vehicleConnected() const;

    // Telemetry Received Flags Getters
    bool hasCameraTelemetry() const { return _hasCameraTelemetry; }
    bool hasVisionTelemetry() const { return _hasVisionTelemetry; }
    bool hasNetworkTelemetry() const { return _hasNetworkTelemetry; }
    bool hasLinksTelemetry() const { return _linksService ? _linksService->hasLinksTelemetry() : false; }
    bool hasSystemTelemetry() const { return _hasSystemTelemetry; }

    // Camera Getters
    int camStatus() const { return _camStatus; }
    QString camName() const { return _camName; }
    QString cameraType() const { return _cameraType; }
    QString connectionPort() const { return _connectionPort; }
    QString serialNumber() const { return _serialNumber; }
    int videoWidth() const { return _videoWidth; }
    int videoHeight() const { return _videoHeight; }
    int videoFps() const { return _videoFps; }
    int depthWidth() const { return _depthWidth; }
    int depthHeight() const { return _depthHeight; }
    int depthFps() const { return _depthFps; }
    int bitrateKbps() const { return _bitrateKbps; }
    int bitrateMaxKbps() const { return _bitrateMaxKbps; }
    int vbvBufferKb() const { return _vbvBufferKb; }
    int rotation() const { return _rotation; }
    int rtspPort() const { return _rtspPort; }
    QString rtspUrlQgc() const { return _rtspUrlQgc; }
    QString rtspUrlController() const { return _rtspUrlController; }
    QString rtspUrlLaptop() const { return _rtspUrlLaptop; }
    QString codec() const { return _codec; }
    QString encoderMode() const { return _encoderMode; }
    bool isDefaultCam() const { return _isDefaultCam; }
    int profileMode() const { return _profileMode; }
    int enableEmitter() const { return _enableEmitter; }
    int cameraStatus() const { return _cameraStatus; }
    int errorCode() const { return _errorCode; }
    int usbSpeedMode() const { return _usbSpeedMode; }

    // Vision Getters
    float confidenceThresh() const { return _confidenceThresh; }
    float inferenceFps() const { return _inferenceFps; }
    int visionInputWidth() const { return _visionInputWidth; }
    int visionInputHeight() const { return _visionInputHeight; }
    int visionVideoFps() const { return _visionVideoFps; }
    int detectionsCount() const { return _detectionsCount; }
    int visionStatusFlags() const { return _visionStatusFlags; }
    QString modelName() const { return _modelName; }
    QString inputSource() const { return _inputSource; }

    // Network Getters
    QString eth0Ip() const { return _eth0Ip; }
    QString eth0Netmask() const { return _eth0Netmask; }
    int eth0Status() const { return _eth0Status; }
    int eth0IsStatic() const { return _eth0IsStatic; }
    quint32 eth0RxKb() const { return _eth0RxKb; }
    quint32 eth0TxKb() const { return _eth0TxKb; }
    QString wlan0Ip() const { return _wlan0Ip; }
    QString wlan0Netmask() const { return _wlan0Netmask; }
    int wlan0Status() const { return _wlan0Status; }
    QString wlan0Ssid() const { return _wlan0Ssid; }
    int wlan0Rssi() const { return _wlan0Rssi; }
    int wlan0Dhcp() const { return _wlan0Dhcp; }
    quint32 wlan0RxKb() const { return _wlan0RxKb; }
    quint32 wlan0TxKb() const { return _wlan0TxKb; }
    QString apIp() const { return _apIp; }
    QString apNetmask() const { return _apNetmask; }
    QString apSsid() const { return _apSsid; }
    int apChannel() const { return _apChannel; }
    int apClientCount() const { return _apClientCount; }
    int apStatus() const { return _apStatus; }
    QString apHwMode() const { return _apHwMode; }
    quint32 uap0RxKb() const { return _uap0RxKb; }
    quint32 uap0TxKb() const { return _uap0TxKb; }
    int dnsmasqStatus() const { return _dnsmasqStatus; }

    // Extended Parameters Getters
    bool ccParametersLoading() const { return _paramService ? _paramService->isLoading() : false; }

    // Links Getters (delegated to CompanionLinksService)
    int fcBaud() const { return _linksService ? _linksService->fcBaud() : 0; }
    int siyiBaud() const { return _linksService ? _linksService->siyiBaud() : 0; }
    QString fcPort() const { return _linksService ? _linksService->fcPort() : QString(); }
    QString siyiPort() const { return _linksService ? _linksService->siyiPort() : QString(); }
    int fcStatus() const { return _linksService ? _linksService->fcStatus() : 0; }
    int siyiStatus() const { return _linksService ? _linksService->siyiStatus() : 0; }
    float fcBitrateKbps() const { return _linksService ? _linksService->fcBitrateKbps() : 0.0f; }
    float fcPacketDropRate() const { return _linksService ? _linksService->fcPacketDropRate() : 0.0f; }
    float siyiBitrateKbps() const { return _linksService ? _linksService->siyiBitrateKbps() : 0.0f; }
    float siyiPacketDropRate() const { return _linksService ? _linksService->siyiPacketDropRate() : 0.0f; }
    quint32 fcBytesRx() const { return _linksService ? _linksService->fcBytesRx() : 0; }
    quint32 fcBytesTx() const { return _linksService ? _linksService->fcBytesTx() : 0; }
    quint32 siyiBytesRx() const { return _linksService ? _linksService->siyiBytesRx() : 0; }
    quint32 siyiBytesTx() const { return _linksService ? _linksService->siyiBytesTx() : 0; }
    int siyiLinkQuality() const { return _linksService ? _linksService->siyiLinkQuality() : 0; }
    int linkStatusFlags() const { return _linksService ? _linksService->linkStatusFlags() : 0; }

    float fcTxRate() const { return _linksService ? _linksService->fcTxRate() : 0.0f; }
    float fcRxRate() const { return _linksService ? _linksService->fcRxRate() : 0.0f; }
    float fcTxRateMax() const { return _linksService ? _linksService->fcTxRateMax() : 0.0f; }
    float fcTxRateMulti() const { return _linksService ? _linksService->fcTxRateMulti() : 0.0f; }
    float fcRxLoss() const { return _linksService ? _linksService->fcRxLoss() : 0.0f; }
    quint32 fcTxErr() const { return _linksService ? _linksService->fcTxErr() : 0; }

    float siyiTxRate() const { return _linksService ? _linksService->siyiTxRate() : 0.0f; }
    float siyiRxRate() const { return _linksService ? _linksService->siyiRxRate() : 0.0f; }
    float siyiTxRateMax() const { return _linksService ? _linksService->siyiTxRateMax() : 0.0f; }
    float siyiTxRateMulti() const { return _linksService ? _linksService->siyiTxRateMulti() : 0.0f; }
    float siyiRxLoss() const { return _linksService ? _linksService->siyiRxLoss() : 0.0f; }
    quint32 siyiTxErr() const { return _linksService ? _linksService->siyiTxErr() : 0; }

    QString transportProtocol() const { return _linksService ? _linksService->transportProtocol() : QStringLiteral("Serial / UART"); }
    QStringList availablePorts() const { return _linksService ? _linksService->availablePorts() : QStringList(); }

    // System Getters
    int cpuUsage() const { return _cpuUsage; }
    int ramUsage() const { return _ramUsage; }
    int diskUsage() const { return _diskUsage; }
    int cpuTemp() const { return _cpuTemp; }
    quint32 uptimeS() const { return _uptimeS; }

    // Toast Getters
    QString lastToastMsg() const { return _lastToastMsg; }
    bool lastToastIsError() const { return _lastToastIsError; }

    // Extended Parameters QML Invokables
    Q_INVOKABLE void requestCcParameters() {
        if (_paramService && companionState().value("connected").toBool()) _paramService->requestParameters();
    }
    QVariantMap cameraStreamState() const { return streamState("camera"); }
    QVariantMap visionStreamState() const { return streamState("vision"); }
    QVariantMap hotspotState() const;
    QVariantMap companionState() const;
    QVariantList serialLinks() const { return _linksService->serialLinks(); }
    /// The single stream mode, CC_STREAM_MODE: 0 Off, 1 Video, 2 Vision. A new mode while a mode operation is pending
    /// replaces it (the CC cancels the old one). Camera/Vision writes are allowed while armed.
    Q_INVOKABLE bool setStreamMode(int mode);
    /// Sends the previous confirmed mode again. Only a mode operation can be cancelled this way.
    Q_INVOKABLE void cancelStreamOperation();
    /// {mode (confirmed, -1 unknown), pendingMode, modeOperation, pending, operation, reason, visionFresh,
    ///  blocked (why a mode SET would be refused now; empty = allowed)}
    QVariantMap streamModeState() const;
    Q_INVOKABLE bool setStreamParameter(const QString& stream, const QString& id, const QVariant& value);
    /// Why the popup of this stream cannot be used: {code, text}; code 0 = usable (see the table in the Fly View spec).
    QVariantMap streamReason(const QString& stream) const;
    /// Port device and baud of any UART the CC reports, in one operation; a device already used by another link is refused.
    Q_INVOKABLE bool setLinkConfig(int index, const QString& port, int baud);
    Q_INVOKABLE bool applyHotspot(const QVariantMap& draft);
    /// MAV_CMD_THACO_SAVE_DEFAULT_CONFIG for "links" (1) or "hotspot" (3): only after a confirmed APPLY of that group.
    Q_INVOKABLE bool saveCompanionGroup(const QString& group);
    /// Ends a links/hotspot APPLY that waits for the CC to come back, as unconfirmed.
    Q_INVOKABLE void cancelCompanionWait();
    /// Links/hotspot writes are locked: the CC reports CC_SYS_STATUS_CONFIG_LOCKED, or the active Vehicle is armed (spec §7).
    bool configLocked() const;
    /// Steps and progress of the operation a popup started: {active, progress 0..1, steps[{direction, text, state, elapsedMs}], error, outcome}.
    /// It stays available after the operation ends, until the next one starts. origin: "stream" (camera and Vision) or "companion".
    Q_INVOKABLE QVariantMap operationState(const QString& origin) const;

    // The CC's COMMAND_ACK for the APPLY_CONFIG this controller sent (see advanceToolbarOperation); noResponse = CC silent.
    void onApplyProgress(uint8_t progress);
    void onApplyResult(uint8_t result, bool noResponse);

    /// One send path to the CC, Vehicle or not: the link its heartbeat came in on (spec §5).
    bool sendToCompanion(const mavlink_message_t& message);
    /// COMMAND_LONG to the CC, sent once: a lost ACK is settled by telemetry/readback, never by a resend.
    bool sendCompanionCommand(uint16_t command, float param1, float param2 = 0);

    // Test-time introspection helpers (available in all builds via friendship)
    bool vehicleAvailable() const { return _activeVehicle != nullptr; }
    int  ccSessionEpoch()   const { return _ccSessionEpoch; }
    bool linksReceived()    const { return _linksService ? _linksService->hasLinksTelemetry() : false; }
    bool linksStale()       const { return _linksStale; }
    bool cameraReceived()   const { return _hasCameraTelemetry; }
    bool networkReceived()  const { return _hasNetworkTelemetry; }
    bool visionReceived()   const { return _hasVisionTelemetry; }
    bool systemReceived()   const { return _hasSystemTelemetry; }
    int  sourceSystemId()   const { return _sourceSystemId; }
    int  sourceComponentId()const { return _sourceComponentId; }

    QVariantMap ccTelemetryLinks() const;
    QVariantMap ccTelemetryCamera() const;
    QVariantMap ccTelemetryNetwork() const;
    QVariantMap ccTelemetryVision() const;
    QVariantMap ccTelemetrySystem() const;

    // Test helpers
    void processMessageForTest(const mavlink_message_t& message, LinkInterface* link = nullptr) { _onMavlinkMessageReceived(message, link); }
    void setSendHookForTest(std::function<bool(const mavlink_message_t&)> hook);
    void resetForTest();
    void forceStaleForTest();
    void setReceivedAgeForTest(const QString& key, qint64 ageMs);



signals:
    void toolbarStateChanged();
    void vehicleConnectedChanged();
    void cameraChanged();
    void visionChanged();
    void networkChanged();
    void linksChanged();
    void availablePortsChanged();
    void ccParametersLoadingChanged();
    void vehicleAvailableChanged();
    void systemChanged();
    void toastChanged();
    void videoRestarted(int mode); ///< after a confirmed mode change QGC's player was stopped (and started unless Off)

private slots:
    void _setActiveVehicle(Vehicle* vehicle);
    void _onMavlinkMessageReceived(const mavlink_message_t& message, LinkInterface* receivedOn = nullptr);

private:
    void _showToast(const QString& msg, bool isError);
    void _clearTelemetryState();
    CompanionTransport transport();
    void onCompanionLost();
    void onCompanionSessionStarted();
    void checkReadbackAfterReconnect();

    QVariantMap streamState(const QString& stream) const;
    bool beginToolbarOperation(const QString& group, const QList<QPair<QString, QVariant>>& values, int subsystem = 0,
                               const QString& origin = QString(), const QStringList& skippedNotes = QStringList());
    struct OperationStep { QString direction; QString text; QString state; qint64 elapsedMs{0}; };
    void addOperationStep(const QString& direction, const QString& text, const QString& state);
    void settleWaitingSteps(const QString& state);
    void publishOperation(const QString& outcome, const QString& error = QString());
    void enterTelemetryWait();
    void failOperationByTimeout(const QString& reason);
    void setAckTimedOut();
    void advanceToolbarOperation();
    void checkToolbarEffect();
    void toolbarTimedOut();
    void endUnconfirmed(const QString& note);
    void recordApplyOutcome(bool confirmed);
    bool modeOperationPending() const;
    QString modeBlockedReason() const;
    bool refuseStream(const QString& why);
    void syncPlayerToMode();
    int _playerMode{-1};             // the confirmed mode QGC's player was last (re)started for; -1 none yet
    void finishToolbarOperation(const QString& error = QString());
    void toolbarParameterSaved(const QString& name, bool success);
    QElapsedTimer _toolbarClock;
    QMap<QString, qint64> _toolbarReceived;
    QMap<QString, QVariantMap> _streamTelemetry;
    int _networkConfigVersion{0};
    int _apPrefixLength{0};
    bool _apDhcpEnabled{false};
    QString _companionName;
    struct ToolbarOperation {
        QString group;
        QList<QPair<QString, QVariant>> values;
        QMap<QString, QVariant> previous;
        QString awaiting;
        int next{0};
        int subsystem{0};
        bool waitingCommand{false};
        bool rollingBack{false};
        bool waitingEffect{false};
        qint64 effectAfter{0};
        QString rollbackError;
        QString origin;                  // popup that started it
        QList<OperationStep> steps;
        double progress{0.0};
        qint64 startedMs{0};
        bool sawInProgress{false};       // the awaited SET got ACK IN_PROGRESS
        int inProgressAcks{0};           // IN_PROGRESS ACKs since the awaited SET
        bool expectCancelAck{false};     // a replaced mode operation: the CC answers it before the new one's first IN_PROGRESS
        bool applySent{false};
        bool resultSeen{false};
        bool refused{false};             // the CC answered the command with a refusal: its active config is unchanged
        bool unconfirmedOnTimeout{false};
        int lastApplyProgress{-1};       // >= 0: the CC's first reply to the APPLY came, so it has the command
        QString lastText;                // last STATUSTEXT of the operation's group
        QMap<QString, QVariant> effectCandidate;
        int epoch{0};
        uint16_t command{0};             // the COMMAND_LONG awaiting the CC's COMMAND_ACK
        int systemId{-1};                // the CC it was sent to
        bool waitingReconnect{false};    // a links/hotspot APPLY lost the CC: no deadline, read back when it returns
        bool readbackRequested{false};
    };
    std::optional<ToolbarOperation> _toolbarOperation;
    QTimer* _toolbarOperationTimer{nullptr};
    QTimer* _setAckTimer{nullptr};
    QMap<QString, QVariantMap> _lastOperation;
    QMap<QString, QString> _toolbarErrors;
    QMap<QString, bool> _unsaved;    // links/hotspot: an APPLY was confirmed and not saved yet
    QMap<QString, bool> _saveBlocked; // a later APPLY was not confirmed, or the hotspot one failed: its candidate may be active
    int _lastCompanionSystemId{-1};  // the CC locked last; _unsaved/_saveBlocked describe it and survive a lock reset
    uint8_t _systemStatus{0};        // CC_TELEMETRY_SYSTEM.system_status (CC_SYS_STATUS bits)

    // SOLID Services
    CompanionLinksService* _linksService = nullptr;
    CompanionParamService* _paramService = nullptr;
    QTimer* _ccSyncTimer = nullptr;
    CompanionMavlinkDispatcher* _dispatcher = nullptr;

    QPointer<Vehicle> _activeVehicle;

    // Telemetry Received Flags
    bool _hasCameraTelemetry  = false;
    bool _hasVisionTelemetry  = false;
    bool _hasNetworkTelemetry = false;
    QTimer* _telemetryWatchdog = nullptr;
    bool _hasSystemTelemetry  = false;

    // Camera state
    int _camStatus = 0;
    QString _camName = "";
    QString _cameraType = "";
    QString _connectionPort = "";
    QString _serialNumber = "";
    int _videoWidth = 0;
    int _videoHeight = 0;
    int _videoFps = 0;
    int _depthWidth = 0;
    int _depthHeight = 0;
    int _depthFps = 0;
    int _bitrateKbps = 0;
    int _bitrateMaxKbps = 0;
    int _vbvBufferKb = 0;
    int _rotation = 0;
    int _rtspPort = 8554;
    QString _rtspUrlQgc = "";
    QString _rtspUrlController = "";
    QString _rtspUrlLaptop = "";
    QString _codec = "";
    QString _encoderMode = "";
    bool _isDefaultCam = false;
    int _profileMode = 0;
    int _enableEmitter = 0;
    int _cameraStatus = 0;
    int _errorCode = 0;
    int _usbSpeedMode = 0;

    // Vision state
    float _confidenceThresh = 0.0f;
    float _inferenceFps = 0.0f;
    int _visionInputWidth = 0;
    int _visionInputHeight = 0;
    int _visionVideoFps = 0;
    int _detectionsCount = 0;
    int _visionStatusFlags = 0;
    QString _modelName = "";
    QString _inputSource = "";

    // Network state
    QString _eth0Ip = "";
    QString _eth0Netmask = "";
    int _eth0Status = 0;
    int _eth0IsStatic = 0;
    quint32 _eth0RxKb = 0;
    quint32 _eth0TxKb = 0;
    QString _wlan0Ip = "";
    QString _wlan0Netmask = "";
    int _wlan0Status = 0;
    QString _wlan0Ssid = "";
    int _wlan0Rssi = 0;
    int _wlan0Dhcp = 0;
    quint32 _wlan0RxKb = 0;
    quint32 _wlan0TxKb = 0;
    QString _apIp = "";
    QString _apNetmask = "";
    QString _apSsid = "";
    int _apChannel = 0;
    int _apClientCount = 0;
    int _apStatus = 0;
    QString _apHwMode = "";
    quint32 _uap0RxKb = 0;
    quint32 _uap0TxKb = 0;
    int _dnsmasqStatus = 0;
    int _apIeee80211n = 0;
    int _apWmmEnabled = 0;
    int _apWpa = 0;
    QString _apWpaPassphrase;
    QString _apKeyMgmt;

    // System state
    int _cpuUsage = 0;
    int _ramUsage = 0;
    int _diskUsage = 0;
    int _cpuTemp = 0;
    quint32 _uptimeS = 0;

    // Toast
    QString _lastToastMsg;
    bool _lastToastIsError = false;

    // Source tracking (set when first CC telemetry received)
    int _sourceSystemId = -1;
    int _sourceComponentId = -1;
    bool _linksStale = false;
    WeakLinkInterfacePtr _ccLink;     // the link the CC heartbeat came in on
    int _ccSessionEpoch = 0;          // +1 when the CC heartbeat starts (first, or back after > 3.5 s)
    bool _ccLost = false;
    std::function<bool(const mavlink_message_t&)> _sendHook;

};

