pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QGroundControl
import QGroundControl.Controls

// Three-position stream mode switch: Video | Off | Vision (CC_STREAM_MODE 1 | 0 | 2).
// It shows the mode the CC confirmed; while a change is pending (busy) it is dimmed and does not move.
// A tap emits requested(mode); the switch sends it itself, asking first when Off is picked while armed.
Row {
    id: control
    property int mode: -1
    property bool busy: false
    property bool fontSmall: false
    signal requested(int mode)

    readonly property var segments: [{value: 1, text: qsTr("Video")}, {value: 0, text: qsTr("Off")}, {value: 2, text: qsTr("Vision")}]
    opacity: (control.busy || !control.enabled) ? 0.5 : 1.0
    spacing: 0
    QGCPalette { id: qgcPal; colorGroupEnabled: control.enabled }

    onRequested: (requestedMode) => {
        const vehicle = QGroundControl.multiVehicleManager.activeVehicle
        if (requestedMode === 0 && vehicle && vehicle.armed) {
            QGroundControl.showMessageDialog(control, qsTr("Tắt camera khi đang bay?"),
                                             qsTr("Video sẽ dừng cho tới khi bật lại Video hoặc Vision."),
                                             Dialog.Yes | Dialog.No, function() { CompanionController.setStreamMode(0) })
        } else {
            CompanionController.setStreamMode(requestedMode)
        }
    }

    Repeater {
        model: control.segments
        delegate: Rectangle {
            id: segment
            required property var modelData
            required property int index
            readonly property bool selected: control.mode === segment.modelData.value
            width: label.implicitWidth + ScreenTools.defaultFontPixelWidth * (control.fontSmall ? 1 : 2)
            height: label.implicitHeight + ScreenTools.defaultFontPixelHeight * (control.fontSmall ? 0.2 : 0.5)
            radius: height / 4
            color: segment.selected ? (segment.modelData.value === 0 ? qgcPal.buttonHighlight : qgcPal.colorGreen) : qgcPal.button
            border.color: qgcPal.text
            border.width: 1

            QGCLabel {
                id: label
                anchors.centerIn: parent
                text: segment.modelData.text
                font.pointSize: control.fontSmall ? ScreenTools.smallFontPointSize : ScreenTools.defaultFontPointSize
                font.bold: segment.selected
                color: segment.selected ? qgcPal.buttonHighlightText : qgcPal.buttonText
            }
            MouseArea {
                anchors.fill: parent
                enabled: control.enabled && !segment.selected
                onClicked: control.requested(segment.modelData.value)
            }
        }
    }
}
