pragma ComponentBehavior: Bound
import QtQuick
import QGroundControl

Item {
    id: control

    property bool checked: false
    signal toggled(bool checked)
    signal clicked()

    implicitWidth: 46
    implicitHeight: 26

    QGCPalette { id: qgcPal; colorGroupEnabled: control.enabled }

    readonly property color trackOnColor:  "#34C759"
    readonly property color trackOffColor: qgcPal.globalTheme === QGCPalette.Light ? "#E5E5EA" : "#39393D"
    readonly property color knobColor:     "#FFFFFF"
    readonly property real  _inset:        2.5
    readonly property real  knobDiameter:  control.height - (control._inset * 2)

    opacity: control.enabled ? 1.0 : 0.45
    Behavior on opacity { NumberAnimation { duration: 150 } }

    Rectangle {
        id: track
        anchors.fill: parent
        radius: height / 2
        color: control.checked ? control.trackOnColor : control.trackOffColor
        border.color: control.checked ? "transparent" : (qgcPal.globalTheme === QGCPalette.Light ? "#D1D1D6" : "#48484A")
        border.width: control.checked ? 0 : 1

        Behavior on color {
            ColorAnimation { duration: 180; easing.type: Easing.InOutQuad }
        }

        // Thumb / Knob
        Rectangle {
            id: knob
            anchors.verticalCenter: parent.verticalCenter
            x: control.checked ? (track.width - width - control._inset) : control._inset
            width: mouseArea.pressed ? (control.knobDiameter + 2) : control.knobDiameter
            height: control.knobDiameter
            radius: height / 2
            color: control.knobColor

            // Soft tactile rim
            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: "transparent"
                border.color: Qt.rgba(0, 0, 0, 0.12)
                border.width: 1
            }

            Behavior on x {
                NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
            }
            Behavior on width {
                NumberAnimation { duration: 120 }
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        enabled: control.enabled
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            control.toggled(!control.checked)
            control.clicked()
        }
    }
}
