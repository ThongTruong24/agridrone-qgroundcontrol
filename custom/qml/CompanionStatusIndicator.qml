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
            return !!(s && s.fresh && s.current && s.current.connected)
        } else if (control.section === "vision") {
            const s = CompanionController.visionStreamState
            return !!(s && s.fresh && s.current && s.current.connected)
        }
        return false
    }

    // No telemetry at all (vehicle, Companion link or the node itself) is different from a device reporting itself off.
    readonly property var reason: control.section === "camera" ? CompanionController.cameraStreamState.reason
                                : control.section === "vision" ? CompanionController.visionStreamState.reason : null
    readonly property bool noData: control.section === "companion" ? !control.connected
                                 : !!(control.reason && control.reason.code >= 1 && control.reason.code <= 5)

    readonly property string sectionName: control.section === "camera" ? qsTr("Video") : control.section === "vision" ? qsTr("Vision") : qsTr("Companion")

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
                text: control.noData ? qsTr("No data") : control.connected ? qsTr("Connected") : qsTr("Disconnected")
                color: control.noData ? qgcPal.text : control.connected ? qgcPal.colorGreen : qgcPal.colorRed
                opacity: control.noData ? 0.6 : 1.0
                font.pointSize: ScreenTools.smallFontPointSize
            }

            QGCLabel {
                anchors.horizontalCenter: parent.horizontalCenter
                text: control.sectionName
                color: qgcPal.text
                font.pointSize: ScreenTools.smallFontPointSize
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        // MainWindow provides this context, as for the existing GPS indicator.
        // qmllint disable unqualified
        onClicked: mainWindow.showIndicatorDrawer(control.section === "companion" ? companionPopup : streamPopup, control)
        // qmllint enable unqualified
    }

    Component { id: streamPopup; StreamStatusPage { stream: control.section } }
    Component { id: companionPopup; CompanionStatusPage {} }
    QGCPalette { id: qgcPal }
}
