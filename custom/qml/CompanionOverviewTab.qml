import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import Custom.AgriDrone

ColumnLayout {
    id: root

    Layout.fillWidth: true
    spacing: ScreenTools.defaultFontPixelHeight

    QGCPalette { id: qgcPal }

    function formatUptime(seconds) {
        if (!CompanionController.hasSystemTelemetry || !seconds || seconds <= 0) return "--"
        var hrs = Math.floor(seconds / 3600)
        var mins = Math.floor((seconds % 3600) / 60)
        var secs = seconds % 60
        var res = ""
        if (hrs > 0) res += hrs + "h "
        if (mins > 0 || hrs > 0) res += mins + "m "
        res += secs + "s"
        return res
    }

    // —— 1. Live System Metrics Group ———————————————————————————————————
    SettingsGroupLayout {
        Layout.fillWidth: true
        heading: qsTr("System Overview & Health")

        // Status Banner Row
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth * 1.5

            Rectangle {
                width:  ScreenTools.defaultFontPixelHeight * 0.75
                height: width
                radius: width / 2
                color:  CompanionController.hasSystemTelemetry ? qgcPal.colorGreen : (CompanionController.vehicleConnected ? qgcPal.colorOrange : qgcPal.colorRed)
            }

            QGCLabel {
                text: CompanionController.vehicleConnected
                      ? (CompanionController.hasSystemTelemetry
                         ? qsTr("MAVLink Companion Agent Active (Target Comp ID: 191)")
                         : qsTr("MAVLink Connected — Waiting for System Telemetry..."))
                      : qsTr("No MAVLink Connection")
                font.bold: true
                color: qgcPal.text
            }

            Item { Layout.fillWidth: true }

            QGCLabel {
                text: CompanionController.hasSystemTelemetry
                      ? qsTr("ONLINE")
                      : (CompanionController.vehicleConnected ? qsTr("STANDBY") : qsTr("OFFLINE"))
                color: CompanionController.hasSystemTelemetry ? qgcPal.colorGreen : (CompanionController.vehicleConnected ? qgcPal.colorOrange : qgcPal.colorRed)
                font.bold: true
            }
        }

        // Metrics Grid
        GridLayout {
            Layout.fillWidth: true
            columns: ScreenTools.isMobile ? 2 : 4
            rowSpacing: ScreenTools.defaultFontPixelHeight * 0.8
            columnSpacing: ScreenTools.defaultFontPixelWidth * 2

            // CPU
            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("CPU Usage"); font.pointSize: ScreenTools.smallFontPointSize; color: qgcPal.text; opacity: 0.7 }
                RowLayout {
                    spacing: 6
                    QGCLabel {
                        text: CompanionController.hasSystemTelemetry && CompanionController.cpuUsage > 0
                              ? (CompanionController.cpuUsage + "%")
                              : "--"
                        font.bold: true
                        font.pointSize: ScreenTools.mediumFontPointSize
                        color: CompanionController.hasSystemTelemetry && CompanionController.cpuUsage > 80 ? qgcPal.colorRed : qgcPal.text
                    }
                }
            }

            // RAM
            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("RAM Usage"); font.pointSize: ScreenTools.smallFontPointSize; color: qgcPal.text; opacity: 0.7 }
                RowLayout {
                    spacing: 6
                    QGCLabel {
                        text: CompanionController.hasSystemTelemetry && CompanionController.ramUsage > 0
                              ? (CompanionController.ramUsage + "%")
                              : "--"
                        font.bold: true
                        font.pointSize: ScreenTools.mediumFontPointSize
                        color: CompanionController.hasSystemTelemetry && CompanionController.ramUsage > 85 ? qgcPal.colorRed : qgcPal.text
                    }
                }
            }

            // Temp
            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("CPU Temperature"); font.pointSize: ScreenTools.smallFontPointSize; color: qgcPal.text; opacity: 0.7 }
                RowLayout {
                    spacing: 6
                    QGCLabel {
                        text: CompanionController.hasSystemTelemetry && CompanionController.cpuTemp > 0
                              ? (CompanionController.cpuTemp + " °C")
                              : "--"
                        font.bold: true
                        font.pointSize: ScreenTools.mediumFontPointSize
                        color: CompanionController.hasSystemTelemetry && CompanionController.cpuTemp > 75 ? qgcPal.colorRed : (CompanionController.hasSystemTelemetry && CompanionController.cpuTemp > 65 ? qgcPal.colorOrange : qgcPal.text)
                    }
                }
            }

            // Uptime
            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("System Uptime"); font.pointSize: ScreenTools.smallFontPointSize; color: qgcPal.text; opacity: 0.7 }
                QGCLabel {
                    text: formatUptime(CompanionController.uptimeS)
                    font.bold: true
                    font.pointSize: ScreenTools.mediumFontPointSize
                }
            }
        }

        // Quick Control Actions
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCButton {
                text: qsTr("Restart Agent (All)")
                enabled: false // Subsystem 0 (All) is unsupported in Milestone A; invoke per subsystem tab
                onClicked: CompanionController.applyConfig(0, true)
            }

            QGCButton {
                text: qsTr("Save Defaults (All)")
                enabled: false // Subsystem 0 (All) is unsupported in Milestone A; invoke per subsystem tab
                onClicked: CompanionController.saveDefaultConfig(0)
            }

            QGCButton {
                text: qsTr("Restore Defaults (All)")
                enabled: false // Subsystem 0 (All) is unsupported in Milestone A; invoke per subsystem tab
                onClicked: CompanionController.restoreDefaultConfig(0)
            }

            Item { Layout.fillWidth: true }
        }
    }

    // —— 2. MAVLink Interactive CLI Console —————————————————————————————
    SettingsGroupLayout {
        Layout.fillWidth: true
        heading: qsTr("MAVLink Interactive CLI Console")

        // Preset Command Chips
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth * 0.8

            QGCLabel {
                text: qsTr("Quick Commands:")
                font.pointSize: ScreenTools.smallFontPointSize
                opacity: 0.8
            }

            Repeater {
                model: ["status", "ping", "ports", "restart", "help"]
                QGCButton {
                    text: modelData
                    pointSize: ScreenTools.smallFontPointSize
                    enabled: CompanionController.vehicleConnected
                    onClicked: {
                        cliInput.text = modelData
                        executeCurrentCommand()
                    }
                }
            }

            Item { Layout.fillWidth: true }

            QGCButton {
                text: qsTr("Clear Log")
                pointSize: ScreenTools.smallFontPointSize
                onClicked: cliLogModel.clear()
            }
        }

        // Console Output Terminal
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 14
            color: qgcPal.windowShade
            border.color: qgcPal.windowShadeDark
            border.width: 1
            radius: 4
            clip: true

            ListModel {
                id: cliLogModel
            }

            Component.onCompleted: {
                cliLogModel.append({
                    timestamp: Qt.formatTime(new Date(), "hh:mm:ss"),
                    dir: "SYS",
                    msg: qsTr("MAVLink Companion CLI Initialized. Type 'help' for available commands.")
                })
            }

            Connections {
                target: CompanionController
                function onMavlinkLogMessage(category, direction, message, severity) {
                    cliLogModel.append({
                        timestamp: Qt.formatTime(new Date(), "hh:mm:ss"),
                        dir: direction,
                        msg: "[" + category + "] " + message
                    })
                    if (cliLogModel.count > 300) {
                        cliLogModel.remove(0)
                    }
                    cliListView.positionViewAtEnd()
                }
            }

            ListView {
                id: cliListView
                anchors.fill: parent
                anchors.margins: 8
                model: cliLogModel
                spacing: 4
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: RowLayout {
                    width: cliListView.width - 16
                    spacing: 8

                    QGCLabel {
                        text: model.timestamp
                        font.family: "Monospace"
                        font.pointSize: ScreenTools.smallFontPointSize * 0.9
                        color: qgcPal.text
                        opacity: 0.5
                    }

                    QGCLabel {
                        text: "[" + model.dir + "]"
                        font.family: "Monospace"
                        font.bold: true
                        font.pointSize: ScreenTools.smallFontPointSize * 0.9
                        color: model.dir === "TX" ? qgcPal.buttonHighlight : (model.dir === "RX" ? qgcPal.colorGreen : qgcPal.colorOrange)
                    }

                    QGCLabel {
                        Layout.fillWidth: true
                        text: model.msg
                        font.family: "Monospace"
                        font.pointSize: ScreenTools.smallFontPointSize * 0.9
                        wrapMode: Text.WrapAnywhere
                        color: qgcPal.text
                    }
                }
            }
        }

        // Command Input Row
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCTextField {
                id: cliInput
                Layout.fillWidth: true
                placeholderText: qsTr("Enter MAVLink CLI command (e.g. status, ping, ports, restart, help)...")
                onAccepted: executeCurrentCommand()
            }

            QGCButton {
                text: qsTr("Send")
                primary: true
                enabled: cliInput.text.trim() !== "" && CompanionController.vehicleConnected
                onClicked: executeCurrentCommand()
            }
        }
    }

    function executeCurrentCommand() {
        var cmd = cliInput.text.trim()
        if (cmd === "") return
        cliLogModel.append({
            timestamp: Qt.formatTime(new Date(), "hh:mm:ss"),
            dir: "TX",
            msg: cmd
        })
        CompanionController.sendCliCommand(cmd)
        cliInput.text = ""
        cliListView.positionViewAtEnd()
    }
}
