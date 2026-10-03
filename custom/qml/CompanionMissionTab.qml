import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import QGroundControl
import QGroundControl.Controls
import Custom.AgriDrone

ColumnLayout {
    id: root

    Layout.fillWidth: true
    spacing: ScreenTools.defaultFontPixelHeight

    QGCPalette { id: qgcPal }

    // —— 1. External XYZ Trigger Telemetry ——————————————————————————————
    SettingsGroupLayout {
        Layout.fillWidth: true
        heading: qsTr("External XYZ Trigger Telemetry")

        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth * 2

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("XYZ Trigger Status"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionController.hasMissionTelemetry ? qsTr("ACTIVE") : "--"
                    font.bold: true
                    font.pointSize: ScreenTools.mediumFontPointSize
                    color: CompanionController.hasMissionTelemetry ? qgcPal.colorGreen : qgcPal.text
                }
            }

            Item { Layout.fillWidth: true }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Last Trigger ID"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionController.hasMissionTelemetry ? ("#" + CompanionController.lastTriggerId) : "--"
                    font.bold: true
                    font.pointSize: ScreenTools.mediumFontPointSize
                }
            }

            Item { Layout.fillWidth: true }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Trigger Timestamp"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionController.hasMissionTelemetry ? (CompanionController.lastTriggerTimeBootMs + " ms") : "--"
                    font.bold: true
                    font.pointSize: ScreenTools.mediumFontPointSize
                }
            }
        }
    }

    // —— 2. Mission Event Audit Log ————————————————————————————————————
    SettingsGroupLayout {
        Layout.fillWidth: true
        heading: qsTr("Mission Event Audit Log")

        CompanionLogPanel {
            Layout.fillWidth: true
            Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 12
            filterPrefix: "MISSION:"
            serviceName: "Mission Trigger"
        }
    }
}
