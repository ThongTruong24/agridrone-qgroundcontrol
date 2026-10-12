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
    property string draftUrl: ""
    property bool urlDirty: false
    property bool readbackRequested: false
    property string localError: ""
    readonly property string title: camera ? qsTr("Vehicle Camera Status") : qsTr("Vehicle Vision Status")
    readonly property bool publishing: streamState.publishStatus === 1 || streamState.publishStatus === 2
    readonly property bool writeAllowed: activeVehicle && !activeVehicle.armed && !activeVehicle.vehicleLinkManager.communicationLost
    readonly property bool editable: writeAllowed && streamState.available && !streamState.pending && !CompanionController.ccParametersLoading
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
            LabelledLabel { label: qsTr("RTSP publisher"); labelText: page.streamState.available ? [qsTr("Stopped"), qsTr("Connecting"), qsTr("Connected"), qsTr("Error")][page.streamState.publishStatus] : qsTr("N/A") }
            QGCLabel { visible: !page.streamState.available; text: qsTr("Controls require current Companion firmware and telemetry"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
    }
    expandedComponent: Component {
        ColumnLayout {
            Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 34
            spacing: ScreenTools.defaultFontPixelHeight / 2
            QGCLabel { text: qsTr("Config"); font.bold: true }
            RowLayout {
                Layout.fillWidth: true
                spacing: ScreenTools.defaultFontPixelWidth

                QGCLabel {
                    text: page.camera ? qsTr("Camera Stream") : qsTr("Vision Stream")
                    Layout.fillWidth: true
                }

                QGCLabel {
                    text: page.streamState.enabled ? qsTr("ON") : qsTr("OFF")
                    font.bold: true
                    color: page.streamState.enabled ? "#34C759" : qgcPal.text
                }

                CompanionToggleSwitch {
                    id: streamSwitch
                    checked: !!page.streamState.enabled
                    enabled: page.writeAllowed && !CompanionController.ccParametersLoading && page.streamState.available && (page.streamState.enabled || !page.streamState.pending)
                    onToggled: (val) => {
                        page.localError = CompanionController.setStreamEnabled(page.stream, val) ? "" : qsTr("Request unavailable or rejected")
                    }
                }
            }
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
            QGCLabel { text: qsTr("RTSP publish destination") }
            QGCTextField {
                objectName: "rtspDestination"
                Layout.fillWidth: true
                text: page.draftUrl
                readOnly: !page.writeAllowed || page.publishing || page.streamState.pending
                placeholderText: "rtsp://host:8554/stream"
                onTextEdited: { page.draftUrl = text; page.urlDirty = true }
            }
            QGCButton {
                objectName: "rtspConnect"
                text: page.publishing ? qsTr("Disconnect") : qsTr("Connect")
                enabled: page.editable && (page.publishing || page.streamState.enabled)
                onClicked: {
                    var accepted = page.publishing ? CompanionController.disconnectStream(page.stream) : CompanionController.connectStream(page.stream, page.draftUrl)
                    page.localError = accepted ? "" : qsTr("Request unavailable or invalid RTSP URL")
                }
            }
            QGCLabel { text: page.streamState.pending ? qsTr("Waiting for effect…") : page.streamState.error || page.localError; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            QGCLabel { visible: !page.writeAllowed; text: page.activeVehicle && page.activeVehicle.armed ? qsTr("Configuration locked while armed") : qsTr("Configuration requires an active vehicle link"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
            QGCLabel { text: qsTr("Changes apply at runtime. ON requires Connect after OFF."); wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
    }
}
