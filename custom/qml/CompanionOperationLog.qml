pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QGroundControl
import QGroundControl.Controls

// Progress bar and step log of the last operation a popup started (CompanionController.operationState).
ColumnLayout {
    id: root
    property var operation: ({})
    readonly property var steps: operation && operation.steps ? operation.steps : []
    readonly property string outcome: operation && operation.outcome ? operation.outcome : ""
    visible: steps.length > 0
    Layout.fillWidth: true
    spacing: ScreenTools.defaultFontPixelHeight / 4
    QGCPalette { id: qgcPal }

    ProgressBar {
        Layout.fillWidth: true
        from: 0
        to: 1
        value: root.operation && root.operation.progress !== undefined ? root.operation.progress : 0
    }
    Repeater {
        model: root.steps
        delegate: QGCLabel {
            required property var modelData
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pointSize: ScreenTools.smallFontPointSize
            color: modelData.state === "error" ? qgcPal.colorRed : modelData.state === "skipped" ? qgcPal.colorGrey : qgcPal.text
            text: (modelData.state === "done" ? "✓ " : modelData.state === "error" ? "✗ " : modelData.state === "skipped" ? "– " : "… ")
                  + modelData.direction + "  " + modelData.text
        }
    }
    QGCLabel {
        visible: root.outcome === "unconfirmed"
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: qsTr("Chưa xác nhận")
        color: qgcPal.colorOrange
    }
}
