pragma Singleton

import QtQuick
import QGC as QGCNative

// QML-only facade. CompanionController is the sole CC MAVLink receiver.
QtObject {
    id: root
    readonly property var backend: QGCNative.CompanionController
    readonly property var _linksData: backend.ccTelemetryLinks
    readonly property var _cameraData: backend.ccTelemetryCamera
    readonly property var _networkData: backend.ccTelemetryNetwork
    readonly property var _visionData: backend.ccTelemetryVision

    readonly property bool vehicleConnected: backend.vehicleAvailable && backend.sourceComponentId > 0
    readonly property bool hasLinksTelemetry: backend.linksReceived && !backend.linksStale
    readonly property bool hasCameraTelemetry: backend.cameraReceived && !backend.cameraStale
    readonly property bool hasNetworkTelemetry: backend.networkReceived && !backend.networkStale
    readonly property bool hasVisionTelemetry: backend.visionReceived && !backend.visionStale
    readonly property bool hasMissionTelemetry: backend.missionReceived && !backend.missionStale
    readonly property int vehicleEpoch: backend.vehicleEpoch

    readonly property int fcBaud: _linksData.fc_baudrate
    readonly property int siyiBaud: _linksData.siyi_baudrate
    readonly property string fcPort: _linksData.fc_port
    readonly property string siyiPort: _linksData.siyi_port
    readonly property int fcStatus: (_linksData.link_status_flags & 1) ? 2 : 0
    readonly property int siyiStatus: (_linksData.link_status_flags & 2) ? 2 : 0
    readonly property int fcBytesRx: _linksData.fc_bytes_rx
    readonly property int fcBytesTx: _linksData.fc_bytes_tx
    readonly property real fcRxRate: NaN
    readonly property real fcTxRate: NaN
    readonly property real fcRxLoss: NaN
    readonly property var fcTxErr: NaN
    readonly property real siyiRxRate: NaN
    readonly property real siyiTxRate: NaN
    readonly property real siyiRxLoss: NaN
    readonly property var siyiBytesRx: NaN
    readonly property var siyiBytesTx: NaN
    readonly property var siyiTxErr: NaN
    readonly property string transportProtocol: qsTr("N/A")
    readonly property var availablePorts: backend.availablePorts
    readonly property string configStatus: backend.configStatus
    readonly property string configMessage: backend.configMessage

    readonly property int videoWidth: _cameraData.video_width
    readonly property int videoHeight: _cameraData.video_height
    readonly property int videoFps: _cameraData.video_fps
    readonly property int bitrateKbps: _cameraData.bitrate_kbps
    readonly property string rtspUrlQgc: _cameraData.rtsp_url
    readonly property string rtspUrlController: ""
    readonly property var usbSpeedMode: NaN

    readonly property int apChannel: _networkData.ap_channel
    readonly property int apClientCount: _networkData.ap_client_count
    readonly property string apIp: _networkData.ap_ip
    readonly property string apSsid: _networkData.ap_ssid
    readonly property int apStatus: -1
    readonly property string eth0Ip: _networkData.eth0_ip
    readonly property string eth0Netmask: qsTr("N/A")

    readonly property real confidenceThresh: _visionData.confidence_thresh
    readonly property real inferenceFps: _visionData.inference_fps
    readonly property int detectionsCount: _visionData.detections_count
    readonly property string inputSource: _visionData.input_source
    readonly property string modelName: _visionData.model_name

    readonly property int cpuTemp: 0
    readonly property int cpuUsage: 0
    readonly property int ramUsage: 0
    readonly property var lastTriggerId: backend.lastTriggerId
    readonly property var lastTriggerTimeBootMs: backend.lastTriggerTimeBootMs
    readonly property string lastToastMsg: backend.configMessage
    readonly property bool lastToastIsError: backend.configStatus === "Failed" || backend.configStatus === "Timeout"

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
