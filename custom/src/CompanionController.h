#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QPointer>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtCore/QTimer>
#include <QtQmlIntegration/QtQmlIntegration>
#include "QGCMAVLink.h"
#include "Vehicle.h"

#include "CompanionLogService.h"
#include "CompanionLinksService.h"
#include "CompanionParamService.h"
#include "CompanionMavlinkDispatcher.h"

/**
 * @brief Facade controller exposing Companion Computer features to QML (SOLID: Facade pattern)
 */
class CompanionController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    friend class CompanionControllerTest;

    // Vehicle status
    Q_PROPERTY(bool vehicleConnected READ vehicleConnected NOTIFY vehicleConnectedChanged)

    // Extended Parameters (CompID 191)
    Q_PROPERTY(QVariantList ccParameters READ ccParameters NOTIFY ccParametersChanged)
    Q_PROPERTY(bool ccParametersLoading READ ccParametersLoading NOTIFY ccParametersLoadingChanged)
    Q_PROPERTY(int ccModifiedParamCount READ ccModifiedParamCount NOTIFY ccModifiedParamCountChanged)
    Q_PROPERTY(QStringList ccParameterGroups READ ccParameterGroups NOTIFY ccParameterGroupsChanged)

    // Telemetry Received Flags
    Q_PROPERTY(bool hasCameraTelemetry READ hasCameraTelemetry NOTIFY cameraChanged)
    Q_PROPERTY(bool hasVisionTelemetry READ hasVisionTelemetry NOTIFY visionChanged)
    Q_PROPERTY(bool hasNetworkTelemetry READ hasNetworkTelemetry NOTIFY networkChanged)
    Q_PROPERTY(bool hasLinksTelemetry READ hasLinksTelemetry NOTIFY linksChanged)
    Q_PROPERTY(bool hasSystemTelemetry READ hasSystemTelemetry NOTIFY systemChanged)
    Q_PROPERTY(bool hasMissionTelemetry READ hasMissionTelemetry NOTIFY missionChanged)

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

    // Mission Properties (from THACO_EXTERNAL_XYZ_TRIGGER 32000)
    Q_PROPERTY(quint32 lastTriggerId READ lastTriggerId NOTIFY missionChanged)
    Q_PROPERTY(quint32 lastTriggerTimeBootMs READ lastTriggerTimeBootMs NOTIFY missionChanged)

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
    Q_PROPERTY(QString configStatus READ configStatus NOTIFY configStatusChanged)
    Q_PROPERTY(int vehicleEpoch READ vehicleEpoch NOTIFY vehicleEpochChanged)
    Q_PROPERTY(bool vehicleAvailable READ vehicleAvailable NOTIFY vehicleAvailableChanged)
    Q_PROPERTY(QString configMessage READ configMessage NOTIFY configStatusChanged)

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
    ~CompanionController() override = default;

    bool vehicleConnected() const;

    // Telemetry Received Flags Getters
    bool hasCameraTelemetry() const { return _hasCameraTelemetry; }
    bool hasVisionTelemetry() const { return _hasVisionTelemetry; }
    bool hasNetworkTelemetry() const { return _hasNetworkTelemetry; }
    bool hasLinksTelemetry() const { return _linksService ? _linksService->hasLinksTelemetry() : false; }
    bool hasSystemTelemetry() const { return _hasSystemTelemetry; }
    bool hasMissionTelemetry() const { return _hasMissionTelemetry; }

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

    // Mission Getters
    quint32 lastTriggerId() const { return _lastTriggerId; }
    quint32 lastTriggerTimeBootMs() const { return _lastTriggerTimeBootMs; }

    // Extended Parameters Getters
    QVariantList ccParameters() const { return _paramService ? _paramService->paramList() : QVariantList(); }
    bool ccParametersLoading() const { return _paramService ? _paramService->isLoading() : false; }
    int ccModifiedParamCount() const { return _paramService ? _paramService->modifiedCount() : 0; }
    QStringList ccParameterGroups() const { return _paramService ? _paramService->groups() : QStringList(); }

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
    QString configStatus() const { return _configStatus; }

    // System Getters
    int cpuUsage() const { return _cpuUsage; }
    int ramUsage() const { return _ramUsage; }
    int diskUsage() const { return _diskUsage; }
    int cpuTemp() const { return _cpuTemp; }
    quint32 uptimeS() const { return _uptimeS; }

    // Toast Getters
    QString lastToastMsg() const { return _lastToastMsg; }
    bool lastToastIsError() const { return _lastToastIsError; }

    // Q_INVOKABLE Actions for QML
    Q_INVOKABLE void sendCameraConfig(int width, int height, int fps, int bitrateKbps, int rotation, const QString& codec);
    Q_INVOKABLE void sendVisionConfig(float confidenceThresh, const QString& modelName, const QString& inputSource);
    Q_INVOKABLE void sendNetworkConfig(int apChannel, const QString& apSsid, const QString& apPsk, const QString& eth0Ip);
    Q_INVOKABLE void sendLinksConfig(int fcBaud, int siyiBaud, const QString& fcPort, const QString& siyiPort);
    Q_INVOKABLE void setTelemetryLinks(const QString& fcPort, int fcBaud, const QString& siyiPort, int siyiBaud) {
        sendLinksConfig(fcBaud, siyiBaud, fcPort, siyiPort);
    }
    Q_INVOKABLE void applyFcLink(const QString& fcPort, int fcBaud);
    Q_INVOKABLE void applySiyiLink(const QString& siyiPort, int siyiBaud);

    // MAV_CMD Staging & Execution
    Q_INVOKABLE void applyConfig(int subsystemId = 0, bool restartImmediate = true, const QString& originCategory = "");
    Q_INVOKABLE void saveDefaultConfig(int subsystemId = 0);
    Q_INVOKABLE void restoreDefaultConfig(int subsystemId = 0);
    Q_INVOKABLE void sendCliCommand(const QString& cmdText);

    // Extended Parameters QML Invokables
    Q_INVOKABLE void requestCcParameters() {
        if (_paramService) _paramService->requestParameters(_activeVehicle ? _activeVehicle.data() : nullptr);
    }
    Q_INVOKABLE void stageCcParameter(const QString& name, const QVariant& value) {
        if (_paramService) _paramService->stageParameter(name, value);
    }
    Q_INVOKABLE void resetCcParameter(const QString& name) {
        if (_paramService) _paramService->resetParameter(name);
    }
    Q_INVOKABLE void resetCcParameterToDefault(const QString& name) {
        if (_paramService) _paramService->resetToDefault(name);
    }
    Q_INVOKABLE bool exportCcParameters(const QString& filePath) {
        return _paramService ? _paramService->exportParameters(filePath) : false;
    }
    Q_INVOKABLE bool importCcParameters(const QString& filePath) {
        return _paramService ? _paramService->importParameters(filePath) : false;
    }
    Q_INVOKABLE void resetAllModifiedCcParameters() {
        if (_paramService) _paramService->resetAllModified();
    }
    Q_INVOKABLE void saveModifiedCcParameters() {
        if (_paramService) _paramService->saveModifiedParameters(_activeVehicle ? _activeVehicle.data() : nullptr);
    }
    Q_INVOKABLE void setSingleCcParameter(const QString& name, const QVariant& value) {
        if (_paramService) _paramService->sendSingleParamSet(_activeVehicle ? _activeVehicle.data() : nullptr, name, value);
    }

    // Audit Log (delegated to CompanionLogService)
    Q_INVOKABLE QVariantList getLogHistory(const QString& category = QString()) const;
    Q_INVOKABLE void clearLogHistory(const QString& category = QString());

    // Test-time introspection helpers (available in all builds via friendship)
    bool vehicleAvailable() const { return _activeVehicle != nullptr; }
    int  vehicleEpoch()     const { return _vehicleEpoch; }
    bool linksReceived()    const { return _linksService ? _linksService->hasLinksTelemetry() : false; }
    bool linksStale()       const { return _linksStale; }
    bool cameraReceived()   const { return _hasCameraTelemetry; }
    bool networkReceived()  const { return _hasNetworkTelemetry; }
    bool visionReceived()   const { return _hasVisionTelemetry; }
    bool systemReceived()   const { return _hasSystemTelemetry; }
    bool missionReceived()  const { return _hasMissionTelemetry; }
    int  sourceSystemId()   const { return _sourceSystemId; }
    int  sourceComponentId()const { return _sourceComponentId; }
    QString configMessage() const { return _configMessage; }

    QVariantMap ccTelemetryLinks() const;
    QVariantMap ccTelemetryCamera() const;
    QVariantMap ccTelemetryNetwork() const;
    QVariantMap ccTelemetryVision() const;
    QVariantMap ccTelemetrySystem() const;

    bool logMatchesFilter(const QString& prefix, const QString& category, const QString& text) const;

    // UART-specific apply / save (state-machine driven)
    void applyLinksConfig(const QString& fcPort, int fcBaud, const QString& siyiPort, int siyiBaud);
    void saveLinksConfig();
    void _configTimedOut();
    void _confirmationTimedOut();

    // Test helpers
    void processMessageForTest(const mavlink_message_t& message) { _onMavlinkMessageReceived(message); }
    void resetForTest();
    void forceStaleForTest();



signals:
    void vehicleConnectedChanged();
    void cameraChanged();
    void visionChanged();
    void networkChanged();
    void linksChanged();
    void availablePortsChanged();
    void ccParametersChanged();
    void ccParametersLoadingChanged();
    void ccModifiedParamCountChanged();
    void ccParameterGroupsChanged();
    void configStatusChanged();
    void vehicleEpochChanged();
    void vehicleAvailableChanged();
    void systemChanged();
    void missionChanged();
    void toastChanged();
    void commandAckReceived(int command, int result, const QString& message);
    void mavlinkLogMessage(const QString& category, const QString& direction, const QString& message, int severity);
    void logEntriesChanged();

private slots:
    void _setActiveVehicle(Vehicle* vehicle);
    void _onMavlinkMessageReceived(const mavlink_message_t& message);

private:
    void _logMavlink(const QString& category, const QString& direction, const QString& message, int severity);
    void _showToast(const QString& msg, bool isError);
    void _clearTelemetryState();
    void _checkTelemetryConfirmation();

    // SOLID Services
    CompanionLogService* _logService = nullptr;
    CompanionLinksService* _linksService = nullptr;
    CompanionParamService* _paramService = nullptr;
    CompanionMavlinkDispatcher* _dispatcher = nullptr;

    QPointer<Vehicle> _activeVehicle;

    // Telemetry Received Flags
    bool _hasCameraTelemetry  = false;
    bool _hasVisionTelemetry  = false;
    bool _hasNetworkTelemetry = false;
    QTimer* _telemetryWatchdog = nullptr;
    bool _hasSystemTelemetry  = false;

    // Mission flag
    bool _hasMissionTelemetry = false;
    quint32 _lastTriggerId = 0;
    quint32 _lastTriggerTimeBootMs = 0;

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

    QString _lastAppliedCategory = "FC";
    QString _configStatus{"IDLE"};

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
    int _vehicleEpoch = 0;

    // Config message (error text)
    QString _configMessage;
    quint16 _pendingCommand = 0;
    bool _waitingTelemetry = false;

    // Config state timers
    QTimer* _configTimer = nullptr;
    QTimer* _confirmTimer = nullptr;

    // Pending links config
    QString _pendingFcPort;
    int _pendingFcBaud = 0;
    QString _pendingSiyiPort;
    int _pendingSiyiBaud = 0;
};

