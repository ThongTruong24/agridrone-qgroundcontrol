import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGC as QGCNative
import Custom.AgriDrone

Rectangle {
    id: root

    readonly property real _panelPadding: ScreenTools.defaultFontPixelWidth * 1.25

    implicitWidth:  gripperLayout.implicitWidth + (_panelPadding * 2)
    implicitHeight: gripperLayout.implicitHeight + (_panelPadding * 2)
    color:          qgcPal.windowShadeDark
    radius:         10
    antialiasing:   true

    function closeIfUnavailable() {
        if (!QGCNative.AgriDroneController.enabled || !QGCNative.AgriDroneController.anyActuatorAvailable) {
            // The standard ToolStripDropPanel provides this context property.
            // qmllint disable unqualified
            dropPanel.hide()
            // qmllint enable unqualified
        }
    }

    QGCPalette { id: qgcPal }

    ColumnLayout {
        id: gripperLayout

        x:       root._panelPadding
        y:       root._panelPadding
        spacing: ScreenTools.defaultFontHeight / 2

        Repeater {
            model: 4

            delegate: ColumnLayout {
                id: gripperDelegate

                required property int index

                spacing: ScreenTools.defaultFontHeight / 2

                QGCLabel {
                    text:      qsTr("Gripper %1").arg(gripperDelegate.index + 1)
                    font.bold: true
                }

                QGCCheckBoxSlider {
                    Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 18
                    enabled:               QGCNative.AgriDroneController.enabled &&
                                               QGCNative.AgriDroneController.actuatorAvailableStates[gripperDelegate.index]
                    checked:               QGCNative.AgriDroneController.actuatorOnStates[gripperDelegate.index]
                    text:                  checked ? qsTr("ON") : qsTr("OFF")

                    onClicked: QGCNative.AgriDroneController.setActuatorOn(gripperDelegate.index, checked)
                }
            }
        }
    }

    Connections {
        target: QGCNative.AgriDroneController

        function onEnabledChanged() { root.closeIfUnavailable() }

        function onActuatorStatesChanged() { root.closeIfUnavailable() }
    }
}
