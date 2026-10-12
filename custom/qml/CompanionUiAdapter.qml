pragma Singleton

import QtQuick
import QGC as QGCNative

// QML-only facade. CompanionController is the sole CC MAVLink receiver.
QtObject {
    id: root
    readonly property var backend: QGCNative.CompanionController

    readonly property bool vehicleConnected: backend.vehicleConnected
    readonly property bool hasLinksTelemetry: backend.hasLinksTelemetry
    readonly property bool hasCameraTelemetry: backend.hasCameraTelemetry
    readonly property bool hasNetworkTelemetry: backend.hasNetworkTelemetry
    readonly property bool hasVisionTelemetry: backend.hasVisionTelemetry
    readonly property bool hasMissionTelemetry: backend.hasMissionTelemetry
    readonly property bool hasSystemTelemetry: backend.hasSystemTelemetry
    readonly property int vehicleEpoch: backend.vehicleEpoch

    readonly property int fcBaud: backend.fcBaud
    readonly property int siyiBaud: backend.siyiBaud
    readonly property string fcPort: backend.fcPort
    readonly property string siyiPort: backend.siyiPort
    readonly property int fcStatus: backend.fcStatus
    readonly property int siyiStatus: backend.siyiStatus
    readonly property int fcBytesRx: backend.fcBytesRx
    readonly property int fcBytesTx: backend.fcBytesTx
    readonly property real fcRxRate: backend.fcRxRate
    readonly property real fcTxRate: backend.fcTxRate
    readonly property real fcRxLoss: backend.fcRxLoss
    readonly property var fcTxErr: backend.fcTxErr
    readonly property real siyiRxRate: backend.siyiRxRate
    readonly property real siyiTxRate: backend.siyiTxRate
    readonly property real siyiRxLoss: backend.siyiRxLoss
    readonly property var siyiBytesRx: backend.siyiBytesRx
    readonly property var siyiBytesTx: backend.siyiBytesTx
    readonly property var siyiTxErr: backend.siyiTxErr
    readonly property string transportProtocol: backend.transportProtocol
    readonly property var availablePorts: backend.availablePorts
    readonly property string configStatus: backend.configStatus
    readonly property string configMessage: backend.configMessage

    readonly property int videoWidth: backend.videoWidth
    readonly property int videoHeight: backend.videoHeight
    readonly property int videoFps: backend.videoFps
    readonly property int bitrateKbps: backend.bitrateKbps
    readonly property string rtspUrlQgc: backend.rtspUrlQgc
    readonly property string rtspUrlController: backend.rtspUrlController
    readonly property var usbSpeedMode: backend.usbSpeedMode

    readonly property int apChannel: backend.apChannel
    readonly property int apClientCount: backend.apClientCount
    readonly property string apIp: backend.apIp
    readonly property string apSsid: backend.apSsid
    readonly property int apStatus: backend.apStatus
    readonly property string eth0Ip: backend.eth0Ip
    readonly property string eth0Netmask: backend.eth0Netmask

    readonly property real confidenceThresh: backend.confidenceThresh
    readonly property real inferenceFps: backend.inferenceFps
    readonly property int detectionsCount: backend.detectionsCount
    readonly property string inputSource: backend.inputSource
    readonly property string modelName: backend.modelName

    readonly property int cpuTemp: backend.cpuTemp
    readonly property int cpuUsage: backend.cpuUsage
    readonly property int ramUsage: backend.ramUsage
    readonly property var lastTriggerId: backend.lastTriggerId
    readonly property var lastTriggerTimeBootMs: backend.lastTriggerTimeBootMs
    readonly property string lastToastMsg: backend.lastToastMsg
    readonly property bool lastToastIsError: backend.lastToastIsError

    signal linksChanged()
    signal cameraChanged()
    signal networkChanged()
    signal visionChanged()
    signal commandAckReceived(int command, int result, string message)
    signal mavlinkLogMessage(string category, string direction, string message, int severity)

    function applyFcLink(port, baud) {
        backend.applyLinksConfig(port, baud, root.siyiPort, root.siyiBaud)
    }
    function applySiyiLink(port, baud) {
        backend.applyLinksConfig(root.fcPort, root.fcBaud, port, baud)
    }
    function saveDefaultConfig(subsystem) {
        if (subsystem === 1) backend.saveLinksConfig()
    }
    function getLogHistory(category) { return backend.getLogHistory(category || "") }
    function clearLogHistory(category) { backend.clearLogHistory(category || "") }
    function logMatchesFilter(filter, category, message) { return backend.logMatchesFilter(filter, category, message) }

    property Connections backendConnections: Connections {
        target: root.backend
        function onLinksChanged() { root.linksChanged() }
        function onCameraChanged() { root.cameraChanged() }
        function onNetworkChanged() { root.networkChanged() }
        function onVisionChanged() { root.visionChanged() }
        function onCommandAckReceived(command, result) {
            root.commandAckReceived(command, result, root.backend.configMessage)
        }
        function onMavlinkLogMessage(category, direction, message, severity) {
            root.mavlinkLogMessage(category, direction, message, severity)
        }
    }
}
