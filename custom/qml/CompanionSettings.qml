
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import QGroundControl
import QGroundControl.Controls
import Custom.AgriDrone
import QGC

/// Companion Computer Settings — 5-tab panel integrated into QGC AppSettings.
/// Tabs: Telemetry | Networking | Camera | Vision | Mission
Item {
    QGCPalette { id: qgcPal }
    id: root
    anchors.fill: parent

    // —— THACO AgriDrone colour tokens —————————————————————————————
    readonly property color _accentBlue:  qgcPal.buttonHighlight
    readonly property color _accentGreen: qgcPal.colorGreen
    readonly property color _bgDark:      qgcPal.window
    readonly property color _bgCard:      qgcPal.windowShade
    readonly property color _borderColor: qgcPal.windowShadeDark
    readonly property color _textNormal:  qgcPal.text
    readonly property color _textMuted:   qgcPal.text
    readonly property color _textError:   qgcPal.colorRed

    // Persisted connection settings
    property int    _companionPort: 8080
    property string _companionIp:  "192.168.10.1"
    property string _apiBase:       "http://" + root._companionIp + ":" + root._companionPort
    property bool   _connected:     false

    function _testConnection() {
        root._connected = false
        var xhr = new XMLHttpRequest()
        xhr.open("GET", root._apiBase + "/v1/health", true)
        xhr.timeout = 3000
        xhr.onreadystatechange = function() {
            if (xhr.readyState !== 4) return
            root._connected = (xhr.status === 200)
        }
        xhr.send()
    }

    // —— Outer background ——————————————————————————————————————————
    Rectangle {
        anchors.fill: parent
        color:        root._bgDark
        radius:       8

        ColumnLayout {
            anchors { fill: parent; margins: 12 }
            spacing: 10

            // —— Connection bar ————————————————————————————————————
            Rectangle {
                Layout.fillWidth:       true
                Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 2.4
                color:                  root._bgCard
                radius:                 6
                border.color:           root._borderColor
                border.width:           1

                RowLayout {
                    anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                    spacing: 12

                    // Status dot
                    Rectangle {
                        Layout.preferredWidth:  10
                        Layout.preferredHeight: 10
                        radius: 5
                        color: CompanionController.vehicleConnected ? root._accentGreen : "#555"
                        Behavior on color { ColorAnimation { duration: 400 } }
                    }

                    QGCLabel {
                        text:  CompanionController.vehicleConnected
                               ? qsTr("● MAVLink Companion Computer (Comp ID 191)")
                               : qsTr("◌ Waiting for MAVLink...")
                        color: CompanionController.vehicleConnected ? root._accentGreen : root._textMuted
                        font.bold: true
                        font.pointSize: ScreenTools.smallFontPointSize
                    }

                    // System live metrics
                    RowLayout {
                        visible: CompanionController.vehicleConnected && CompanionController.cpuTemp > 0
                        spacing: 12

                        QGCLabel {
                            text: qsTr("|  CPU: %1%").arg(CompanionController.cpuUsage)
                            color: CompanionController.cpuUsage > 80 ? root._textError : root._textNormal
                            font.pointSize: ScreenTools.smallFontPointSize
                        }

                        QGCLabel {
                            text: qsTr("RAM: %1%").arg(CompanionController.ramUsage)
                            color: root._textNormal
                            font.pointSize: ScreenTools.smallFontPointSize
                        }

                        QGCLabel {
                            text: qsTr("CPU Temp: %1°C").arg(CompanionController.cpuTemp)
                            color: CompanionController.cpuTemp > 75 ? root._textError : root._textNormal
                            font.pointSize: ScreenTools.smallFontPointSize
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // Toast message pill
                    QGCLabel {
                        text:  CompanionController.lastToastMsg
                        color: CompanionController.lastToastIsError ? root._textError : root._accentGreen
                        font.bold: true
                        font.pointSize: ScreenTools.smallFontPointSize
                        visible: CompanionController.lastToastMsg !== ""
                    }
                }
            }

            // —— Tab bar ———————————————————————————————————————————
            TabBar {
                id: tabBar
                Layout.fillWidth: true
                background: Rectangle { color: root._bgCard; radius: 6; border.color: root._borderColor; border.width: 1 }

                Repeater {
                    model: [
                        { icon: "qrc:/InstrumentValueIcons/radio.svg",        label: qsTr("Telemetry") },
                        { icon: "qrc:/InstrumentValueIcons/network.svg",      label: qsTr("Networking") },
                        { icon: "qrc:/InstrumentValueIcons/video-camera.svg",  label: qsTr("Camera") },
                        { icon: "qrc:/InstrumentValueIcons/view-show.svg",     label: qsTr("Vision") },
                        { icon: "qrc:/InstrumentValueIcons/target.svg",        label: qsTr("Mission") },
                        { icon: "qrc:/InstrumentValueIcons/tuning.svg",        label: qsTr("Parameters") }
                    ]

                    delegate: TabButton {
                        id: tabBtn
                        required property var modelData
                        required property int index

                        width: Math.max(1, Math.floor(root.width / 6))
                        background: Rectangle {
                            color:  tabBar.currentIndex === tabBtn.index
                                    ? Qt.rgba(30/255, 139/255, 195/255, 0.18)
                                    : "transparent"
                            radius: 5
                            Rectangle {
                                visible: tabBar.currentIndex === tabBtn.index
                                anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                                height: 2
                                color:  root._accentBlue
                            }
                        }
                        contentItem: RowLayout {
                            spacing: 6
                            Item { Layout.fillWidth: true }
                            QGCColoredImage {
                                source: tabBtn.modelData.icon
                                height: ScreenTools.defaultFontPixelHeight * 0.95
                                width: height
                                color: tabBar.currentIndex === tabBtn.index ? root._accentBlue : root._textMuted
                                fillMode: Image.PreserveAspectFit
                                sourceSize.height: height
                            }
                            QGCLabel {
                                text: tabBtn.modelData.label
                                color: tabBar.currentIndex === tabBtn.index ? root._accentBlue : root._textMuted
                                font.pointSize: ScreenTools.smallFontPointSize
                                font.bold: tabBar.currentIndex === tabBtn.index
                                verticalAlignment: Text.AlignVCenter
                            }
                            Item { Layout.fillWidth: true }
                        }
                    }
                }
            }

            // —— Tab content ———————————————————————————————————————
            Item {
                Layout.fillWidth:  true
                Layout.fillHeight: true


                CompanionTelemetryTab  { objectName: "companionTelemetryTab"; anchors.fill: parent; visible: tabBar.currentIndex === 0 }
                CompanionNetworkingTab { anchors.fill: parent; visible: tabBar.currentIndex === 1 }
                CompanionCameraTab     { anchors.fill: parent; visible: tabBar.currentIndex === 2 }
                CompanionVisionTab     { anchors.fill: parent; visible: tabBar.currentIndex === 3 }
                CompanionMissionTab    { anchors.fill: parent; visible: tabBar.currentIndex === 4 }
                CompanionParametersTab { anchors.fill: parent; visible: tabBar.currentIndex === 5 }
            }
        }
    }
}


