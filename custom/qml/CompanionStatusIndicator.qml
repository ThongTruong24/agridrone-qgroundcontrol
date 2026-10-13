pragma ComponentBehavior: Bound
import QtQuick
import QGroundControl
import QGroundControl.Controls
import Custom.AgriDrone

Item {
    id: control
    property string section: "companion"
    property bool showIndicator: true
    width: indicatorRow.width
    anchors.top: parent.top
    anchors.bottom: parent.bottom

    readonly property bool connected: {
        if (control.section === "companion") {
            return !!(CompanionController.companionState && CompanionController.companionState.connected)
        } else if (control.section === "camera") {
            const s = CompanionController.cameraStreamState
            const streaming = !!(s && s.fresh && s.current && s.current.connected)
            // Vision is only "Connected" when its boxes can come: the camera streams and Vision telemetry is fresh.
            return control.modeState.mode === 2 ? streaming && !!control.modeState.visionFresh : streaming
        }
        return false
    }
    readonly property var modeState: CompanionController.streamModeState || ({})
    readonly property bool streamOff: control.section === "camera" && control.modeState.mode === 0

    // No telemetry at all (vehicle, Companion link or the node itself) is different from a device reporting itself off.
    readonly property var reason: control.section === "camera" ? CompanionController.cameraStreamState.reason : null
    readonly property bool noData: control.section === "companion" ? !control.connected
                                 : !!(control.reason && control.reason.code >= 1 && control.reason.code <= 5)


    MouseArea {
        anchors.fill: parent
        // MainWindow provides this context, as for the existing GPS indicator.
        // qmllint disable unqualified
        onClicked: mainWindow.showIndicatorDrawer(control.section === "companion" ? companionPopup : streamPopup, control)
        // qmllint enable unqualified
    }

    Row {
        id: indicatorRow
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        spacing: ScreenTools.defaultFontPixelWidth / 2

        QGCColoredImage {
            id: icon
            width: height
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            source: "/custom/images/" + control.section + ".png"
            fillMode: Image.PreserveAspectFit
            sourceSize.height: height
            opacity: control.connected ? 1.0 : 0.6
            color: qgcPal.text
        }

        Column {
            id: statusColumn
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0

            QGCLabel {
                anchors.horizontalCenter: parent.horizontalCenter
                text: control.noData ? qsTr("No data") : control.streamOff ? qsTr("Off")
                    : control.connected ? qsTr("Connected") : qsTr("Disconnected")
                color: (control.noData || control.streamOff) ? qgcPal.text : control.connected ? qgcPal.colorGreen : qgcPal.colorRed
                opacity: (control.noData || control.streamOff) ? 0.6 : 1.0
                font.pointSize: ScreenTools.smallFontPointSize
            }

            QGCLabel {
                visible: control.section === "companion"
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Companion")
                color: qgcPal.text
                font.pointSize: ScreenTools.smallFontPointSize
            }

            // Moves only when the CC confirms; dimmed while a change is pending.
            StreamModeSwitch {
                visible: control.section === "camera"
                anchors.horizontalCenter: parent.horizontalCenter
                fontSmall: true
                mode: control.modeState.mode !== undefined ? control.modeState.mode : -1
                busy: control.modeState.pendingMode !== control.modeState.mode
                enabled: !control.noData
            }
        }
    }

    Component { id: streamPopup; StreamStatusPage {} }
    Component { id: companionPopup; CompanionStatusPage {} }
    QGCPalette { id: qgcPal }
}
