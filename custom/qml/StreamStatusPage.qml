pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QGroundControl
import QGroundControl.Controls
import Custom.AgriDrone

// One popup for the camera stream: the mode switch (Video | Off | Vision), the active stream's status, and the Video and
// Vision settings. Camera and Vision are outside the arming lock, so everything here stays editable while armed.
ToolIndicatorPage {
    id: page
    QGCPalette { id: qgcPal }
    readonly property var modeState: CompanionController.streamModeState || ({})
    readonly property var cam: CompanionController.cameraStreamState || ({})
    readonly property var vis: CompanionController.visionStreamState || ({})
    readonly property var current: cam.current || ({})
    readonly property var visionCurrent: vis.current || ({})
    readonly property var reason: cam.reason || ({code: 2, text: ""})
    readonly property var visionReason: vis.reason || ({code: 2, text: ""})
    readonly property int mode: modeState.mode !== undefined ? modeState.mode : -1
    property bool readbackRequested: false
    property string localError: ""
    readonly property string title: qsTr("Vehicle Stream Status")
    // Reasons 2-7: nothing to configure yet (no CC heartbeat, node or readback); 9: another operation is running.
    // Camera and Vision need only the CC heartbeat (reason 0 implies it), not a Vehicle (spec §5).
    readonly property bool hideControls: reason.code >= 2 && reason.code <= 7
    readonly property bool editable: reason.code === 0 && !modeState.pending
    readonly property bool visionEditable: editable && visionReason.code === 0
    showExpand: true
    function display(value) { return value === undefined || value === null || value === "" ? qsTr("N/A") : String(value) }
    function modeText(value) { return value === 0 ? qsTr("Off") : value === 1 ? qsTr("Video") : value === 2 ? qsTr("Vision") : qsTr("N/A") }
    function send(stream, id, value) {
        const ok = CompanionController.setStreamParameter(stream, id, value)
        localError = ok ? "" : qsTr("Request unavailable or rejected")
        return ok
    }
    function choices(presets, value) {
        var result = (presets || []).slice()
        if (value !== undefined && value !== null && value !== "" && result.indexOf(String(value)) < 0) result.push(String(value))
        return result
    }
    function codecs(state) {
        const mask = state.supportedCodecs || 0
        return ((state.options && state.options.codec) || []).filter(c => (c === "h264" && (mask & 1)) || (c === "h265" && (mask & 2)))
    }
    function ensureReadback() {
        if (!cam.available) { readbackRequested = false; return }
        if (!readbackRequested && !cam.pending) {
            readbackRequested = true
            CompanionController.requestCcParameters()
        }
    }
    Component.onCompleted: ensureReadback()
    onCamChanged: ensureReadback()

    // One parameter chosen from the CC's option list.
    component ParamCombo: ColumnLayout {
        id: combo
        required property string label
        required property string stream
        required property string paramId
        required property var presets   // the CC's option list; the running value is added when it is not in it
        required property var value
        property bool numeric: true
        property bool allowed: page.editable
        Layout.fillWidth: true
        spacing: 0
        QGCLabel { text: combo.label }
        QGCComboBox {
            Layout.fillWidth: true
            model: page.choices(combo.presets, combo.value)
            currentIndex: model.indexOf(String(combo.value))
            enabled: combo.allowed && combo.value !== undefined && combo.value !== null
            onActivated: page.send(combo.stream, combo.paramId, combo.numeric ? Number(currentText) : currentText)
        }
    }

    // RTSP address of one stream; sent when editing ends.
    component UrlField: ColumnLayout {
        id: urlField
        required property string stream
        required property string paramId
        required property string configured
        property alias objectNameOfEdit: edit.objectName
        Layout.fillWidth: true
        spacing: 0
        QGCLabel { text: qsTr("RTSP publish destination") }
        QGCTextField {
            id: edit
            property bool dirty: false
            Layout.fillWidth: true
            readOnly: !page.editable
            placeholderText: "rtsp://host:8554/camera"
            Component.onCompleted: text = urlField.configured
            onTextEdited: dirty = true
            onEditingFinished: {
                if (dirty && text !== urlField.configured && !page.send(urlField.stream, urlField.paramId, text)) text = urlField.configured
                dirty = false
            }
            Connections {
                target: urlField
                function onConfiguredChanged() { if (!edit.dirty) edit.text = urlField.configured }
            }
        }
    }

    contentComponent: Component {
        ColumnLayout {
            spacing: ScreenTools.defaultFontPixelHeight / 2
            Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 32
            QGCLabel { text: page.title; font.bold: true }
            LabelledLabel { label: qsTr("Mode"); labelText: page.modeText(page.mode) }
            LabelledLabel {
                label: qsTr("Status")
                labelText: !page.cam.fresh ? qsTr("N/A") : page.mode === 0 ? qsTr("Off") : page.current.connected ? qsTr("Connected") : qsTr("Disconnected")
            }
            LabelledLabel { label: qsTr("Name"); labelText: page.display(page.current.name) }
            LabelledLabel { label: qsTr("Resolution"); labelText: page.display(page.current.resolution) }
            LabelledLabel { label: qsTr("FPS"); labelText: page.display(page.current.fps) }
            LabelledLabel { label: qsTr("Bitrate (kbps)"); labelText: page.display(page.current.bitrate) }
            LabelledLabel { label: qsTr("Encode"); labelText: page.display(page.current.codec) }
            LabelledLabel {
                label: qsTr("RTSP publisher")
                labelText: page.cam.available && page.cam.publishStatus >= 0 && page.cam.publishStatus <= 3
                           ? [qsTr("Stopped"), qsTr("Connecting"), qsTr("Connected"), qsTr("Error")][page.cam.publishStatus] : qsTr("N/A")
            }
            LabelledLabel { visible: page.mode === 2; label: qsTr("Model"); labelText: page.display(page.visionCurrent.name) }
            LabelledLabel { visible: page.mode === 2; label: qsTr("Inference FPS"); labelText: page.display(page.visionCurrent.fps) }
            LabelledLabel {
                visible: page.mode === 2
                label: qsTr("Confidence threshold")
                labelText: page.visionCurrent.confidence !== undefined ? Number(page.visionCurrent.confidence).toFixed(2) : qsTr("N/A")
            }
            QGCLabel {
                visible: page.mode === 2 && !page.modeState.visionFresh
                text: qsTr("Vision không phản hồi")
                color: qgcPal.colorOrange
            }
            QGCLabel { visible: page.reason.code !== 0; text: page.reason.text; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
    }
    expandedComponent: Component {
        ScrollView {
            implicitWidth: ScreenTools.defaultFontPixelWidth * 36
            implicitHeight: Math.min(contentHeight, ScreenTools.screenHeight * 0.65)
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: ScreenTools.defaultFontPixelHeight / 2
                QGCLabel { visible: page.hideControls; text: page.reason.text; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                ColumnLayout {
                    visible: !page.hideControls
                    Layout.fillWidth: true
                    spacing: ScreenTools.defaultFontPixelHeight / 2

                    StreamModeSwitch {
                        objectName: "streamModeSwitch"
                        mode: page.mode
                        busy: page.modeState.pendingMode !== page.mode
                        enabled: !page.modeState.blocked
                    }
                    CompanionOperationLog { operation: page.modeState.operation || ({}) }
                    // Only a mode change can be cancelled: a stuck rebuild of the running settings is not undone by resending the mode.
                    QGCButton {
                        objectName: "cancelStreamOperation"
                        visible: !!page.modeState.modeOperation
                        text: qsTr("Hủy")
                        onClicked: CompanionController.cancelStreamOperation()
                    }
                    QGCLabel { visible: !!page.modeState.blocked; text: page.modeState.blocked || ""; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    QGCLabel { visible: page.localError !== ""; text: page.localError; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                    QGCLabel { text: qsTr("Cấu hình Video"); font.bold: true; objectName: "videoConfig" }
                    ParamCombo {
                        label: qsTr("Resolution"); stream: "camera"; paramId: "CC_CAM_RES"; numeric: false
                        value: page.cam.configured ? page.cam.configured.resolution : undefined
                        presets: page.cam.options ? page.cam.options.resolution : []
                    }
                    ParamCombo {
                        label: qsTr("FPS"); stream: "camera"; paramId: "CC_CAM_FPS"
                        value: page.cam.configured ? page.cam.configured.fps : undefined
                        presets: page.cam.options ? page.cam.options.fps : []
                    }
                    ParamCombo {
                        label: qsTr("Bitrate (kbps)"); stream: "camera"; paramId: "CC_CAM_BR"
                        value: page.cam.configured ? page.cam.configured.bitrate : undefined
                        presets: page.cam.options ? page.cam.options.bitrate : []
                    }
                    ParamCombo {
                        label: qsTr("Encode"); stream: "camera"; paramId: "CC_CAM_CODEC"; numeric: false
                        value: page.cam.configured ? page.cam.configured.codec : undefined
                        presets: page.codecs(page.cam)
                    }
                    UrlField { stream: "camera"; paramId: "CC_CAM_PUB_URL"; configured: page.cam.configuredUrl || ""; objectNameOfEdit: "rtspDestination" }

                    QGCLabel { text: qsTr("Cấu hình Vision"); font.bold: true; objectName: "visionConfig" }
                    ParamCombo {
                        label: qsTr("Resolution"); stream: "vision"; paramId: "CC_VIS_RES"; numeric: false
                        value: page.vis.configured ? page.vis.configured.resolution : undefined
                        presets: page.vis.options ? page.vis.options.resolution : []
                    }
                    ParamCombo {
                        label: qsTr("Bitrate (kbps)"); stream: "vision"; paramId: "CC_VIS_BR"
                        value: page.vis.configured ? page.vis.configured.bitrate : undefined
                        presets: page.vis.options ? page.vis.options.bitrate : []
                    }
                    ParamCombo {
                        label: qsTr("Encode"); stream: "vision"; paramId: "CC_VIS_CODEC"; numeric: false
                        value: page.vis.configured ? page.vis.configured.codec : undefined
                        presets: page.codecs(page.vis)
                    }
                    UrlField { stream: "vision"; paramId: "CC_VIS_OUT_URL"; configured: page.vis.configuredUrl || ""; objectNameOfEdit: "visionRtspDestination" }
                    ParamCombo {
                        label: qsTr("Inference FPS"); stream: "vision"; paramId: "CC_VIS_FPS"; allowed: page.visionEditable
                        value: page.vis.configured ? page.vis.configured.fps : undefined
                        presets: page.vis.options ? page.vis.options.fps : []
                    }
                    ParamCombo {
                        objectName: "visionConfidence"
                        label: qsTr("Confidence threshold"); stream: "vision"; paramId: "CC_VIS_CONF"; allowed: page.visionEditable
                        value: page.vis.confidence !== undefined && page.vis.confidence !== null && page.vis.confidence !== ""
                               ? Number(page.vis.confidence).toFixed(2) : undefined
                        presets: Array.from({length: 19}, (_, i) => ((i + 1) * 5 / 100).toFixed(2)) // 0.05 .. 0.95 as in the schema
                    }
                    QGCLabel {
                        visible: page.visionReason.code !== 0
                        text: page.visionReason.text
                        font.pointSize: ScreenTools.smallFontPointSize
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }
        }
    }
}
