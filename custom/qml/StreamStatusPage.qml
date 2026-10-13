pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QGroundControl
import QGroundControl.Controls

ToolIndicatorPage {
    id: page
    QGCPalette { id: qgcPal }
    property string stream: "camera"
    readonly property bool camera: stream === "camera"
    readonly property var streamState: camera ? CompanionController.cameraStreamState : CompanionController.visionStreamState
    readonly property var current: streamState.current || ({})
    readonly property var configured: streamState.configured || ({})
    readonly property var options: streamState.options || ({resolution: [], fps: [], bitrate: [], codec: []})
    readonly property var reason: streamState.reason || ({code: 1, text: ""})
    readonly property var operation: streamState.operation || ({})
    property string draftUrl: ""
    property bool urlDirty: false
    property bool readbackRequested: false
    property string localError: ""
    readonly property string title: camera ? qsTr("Vehicle Video Status") : qsTr("Vehicle Vision Status")
    readonly property bool writeAllowed: activeVehicle && !activeVehicle.armed && !activeVehicle.vehicleLinkManager.communicationLost
    // Reasons 1-7: nothing to configure yet (no vehicle, link, node or readback); 8 and 9: shown but locked.
    readonly property bool hideControls: reason.code >= 1 && reason.code <= 7
    readonly property bool editable: reason.code === 0 && writeAllowed
    showExpand: true
    function display(value) { return value === undefined || value === null || value === "" ? qsTr("N/A") : String(value) }
    function send(id, value) {
        localError = CompanionController.setStreamParameter(stream, id, value) ? "" : qsTr("Request unavailable or rejected")
    }
    function choices(presets, value) {
        var result = presets.slice()
        if (value !== undefined && value !== null && result.indexOf(String(value)) < 0) result.push(String(value))
        return result
    }
    function ensureReadback() {
        if (!streamState || !streamState.available) { readbackRequested = false; return }
        if (!readbackRequested && activeVehicle && !streamState.pending) {
            readbackRequested = true
            CompanionController.requestCcParameters()
        }
    }
    Component.onCompleted: { draftUrl = streamState.configuredUrl || ""; ensureReadback() }
    onActiveVehicleChanged: { readbackRequested = false; ensureReadback() }
    onStreamStateChanged: {
        ensureReadback()
        if (!urlDirty) draftUrl = streamState.configuredUrl || ""
        else if (!streamState.pending && streamState.configuredUrl === draftUrl) urlDirty = false
    }
    contentComponent: Component {
        ColumnLayout {
            spacing: ScreenTools.defaultFontPixelHeight / 2
            Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 32
            QGCLabel { text: page.title; font.bold: true }
            LabelledLabel { label: qsTr("Status"); labelText: page.streamState.fresh ? (page.current.connected ? qsTr("Connected") : qsTr("Disconnected")) : qsTr("N/A") }
            LabelledLabel { label: page.camera ? qsTr("Name") : qsTr("Model"); labelText: page.display(page.current.name) }
            LabelledLabel { label: qsTr("Resolution"); labelText: page.display(page.current.resolution) }
            LabelledLabel { label: page.camera ? qsTr("FPS") : qsTr("Inference FPS"); labelText: page.display(page.current.fps) }
            LabelledLabel { visible: !page.camera; label: qsTr("Output FPS"); labelText: page.display(page.current.outputFps) }
            LabelledLabel { label: qsTr("Bitrate (kbps)"); labelText: page.display(page.current.bitrate) }
            LabelledLabel { label: qsTr("Encode"); labelText: page.display(page.current.codec) }
            LabelledLabel {
                label: qsTr("RTSP publisher")
                labelText: page.streamState.available && page.streamState.publishStatus >= 0 && page.streamState.publishStatus <= 3
                           ? [qsTr("Stopped"), qsTr("Connecting"), qsTr("Connected"), qsTr("Error")][page.streamState.publishStatus] : qsTr("N/A")
            }
            QGCLabel { visible: page.reason.code !== 0; text: page.reason.text; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
    }
    expandedComponent: Component {
        ColumnLayout {
            Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 34
            spacing: ScreenTools.defaultFontPixelHeight / 2
            QGCLabel { text: qsTr("Config"); font.bold: true }
            QGCLabel { visible: page.hideControls; text: page.reason.text; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            ColumnLayout {
                visible: !page.hideControls
                Layout.fillWidth: true
                spacing: ScreenTools.defaultFontPixelHeight / 2
                QGCLabel { text: qsTr("Resolution") }
                QGCComboBox {
                    Layout.fillWidth: true
                    model: page.choices(page.options.resolution, page.configured.resolution)
                    currentIndex: model.indexOf(String(page.configured.resolution))
                    enabled: page.editable && page.configured.resolution !== undefined
                    onActivated: page.send(page.camera ? "CC_CAM_RES" : "CC_VIS_RES", currentText)
                }
                QGCLabel { text: page.camera ? qsTr("Output FPS") : qsTr("Configured inference FPS") }
                QGCComboBox {
                    Layout.fillWidth: true
                    model: page.choices(page.options.fps, page.configured.fps)
                    currentIndex: model.indexOf(String(page.configured.fps))
                    enabled: page.editable && page.configured.fps !== undefined
                    onActivated: page.send(page.camera ? "CC_CAM_FPS" : "CC_VIS_FPS", Number(currentText))
                }
                QGCLabel { text: qsTr("Bitrate (kbps)") }
                QGCComboBox {
                    Layout.fillWidth: true
                    model: page.choices(page.options.bitrate, page.configured.bitrate)
                    currentIndex: model.indexOf(String(page.configured.bitrate))
                    enabled: page.editable && page.configured.bitrate !== undefined
                    onActivated: page.send(page.camera ? "CC_CAM_BR" : "CC_VIS_BR", Number(currentText))
                }
                QGCLabel { text: qsTr("Encode") }
                QGCComboBox {
                    Layout.fillWidth: true
                    model: page.choices(page.options.codec.filter(c => (c === "h264" && (page.streamState.supportedCodecs & 1)) || (c === "h265" && (page.streamState.supportedCodecs & 2))), page.configured.codec)
                    currentIndex: model.indexOf(String(page.configured.codec))
                    enabled: page.editable && page.configured.codec !== undefined && (page.streamState.supportedCodecs & 3) !== 0
                    onActivated: page.send(page.camera ? "CC_CAM_CODEC" : "CC_VIS_CODEC", currentText)
                }
                QGCLabel { visible: !page.camera; text: qsTr("Confidence threshold") }
                QGCComboBox {
                    visible: !page.camera
                    objectName: "visionConfidence"
                    Layout.fillWidth: true
                    readonly property var presets: Array.from({length: 19}, (_, i) => ((i + 1) * 5 / 100).toFixed(2)) // 0.05 .. 0.95 as in the schema
                    readonly property string running: page.streamState.confidence !== undefined && page.streamState.confidence !== null && page.streamState.confidence !== ""
                                                      ? Number(page.streamState.confidence).toFixed(2) : ""
                    model: page.choices(presets, running)
                    currentIndex: model.indexOf(running)
                    enabled: page.editable && running !== ""
                    onActivated: page.send("CC_VIS_CONF", Number(currentText))
                }
                QGCLabel { text: qsTr("RTSP publish destination") }
                QGCTextField {
                    objectName: "rtspDestination"
                    Layout.fillWidth: true
                    text: page.draftUrl
                    readOnly: !page.writeAllowed
                    placeholderText: "rtsp://host:8554/stream"
                    onTextEdited: { page.draftUrl = text; page.urlDirty = true }
                }
                QGCLabel {
                    visible: !!page.streamState.enabled && page.draftUrl !== (page.streamState.configuredUrl || "")
                    text: qsTr("Địa chỉ mới sẽ được dùng khi bật lại")
                    font.pointSize: ScreenTools.smallFontPointSize
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                CompanionOperationLog { operation: page.operation }
                QGCLabel { visible: page.reason.code === 8 || page.reason.code === 9; text: page.reason.text; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                QGCLabel { visible: page.localError !== ""; text: page.localError; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
        }
    }
}
