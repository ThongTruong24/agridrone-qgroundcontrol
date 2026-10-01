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

    property real   _activeConf:   0.0
    property real   _draftConf:    0.0
    property string _activeModel:  ""
    property string _draftModel:   ""
    property string _activeSource: ""
    property string _draftSource:  ""

    readonly property bool hasPendingChanges: (
        CompanionUiAdapter.hasVisionTelemetry &&
        (Math.abs(_draftConf - _activeConf) > 0.01 ||
         _draftModel !== _activeModel ||
         _draftSource !== _activeSource)
    )

    function undoChanges() {
        _draftConf = _activeConf
        _draftModel = _activeModel
        _draftSource = _activeSource
    }

    Connections {
        target: CompanionUiAdapter
        function onVisionChanged() {
            if (CompanionUiAdapter.hasVisionTelemetry) {
                root._activeConf = CompanionUiAdapter.confidenceThresh
                root._draftConf = CompanionUiAdapter.confidenceThresh
                root._activeModel = CompanionUiAdapter.modelName !== "" ? CompanionUiAdapter.modelName : "--"
                root._draftModel = root._activeModel
                root._activeSource = CompanionUiAdapter.inputSource !== "" ? CompanionUiAdapter.inputSource : "--"
                root._draftSource = root._activeSource
            } else {
                root._activeConf = 0.0
                root._draftConf = 0.0
                root._activeModel = ""
                root._draftModel = ""
                root._activeSource = ""
                root._draftSource = ""
            }
        }
    }

    // —— 1. Vision Detection & Model Pipeline ——————————————————————————
    SettingsGroupLayout {
        Layout.fillWidth: true
        heading: qsTr("Vision Detection & Model Pipeline")

        // Live Metrics Row
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth * 2

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Inference Speed"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionUiAdapter.hasVisionTelemetry && CompanionUiAdapter.inferenceFps > 0
                          ? (CompanionUiAdapter.inferenceFps.toFixed(1) + " FPS")
                          : "--"
                    font.bold: true
                    font.pointSize: ScreenTools.mediumFontPointSize
                    color: CompanionUiAdapter.hasVisionTelemetry ? "#2ECC71" : qgcPal.text
                }
            }

            Item { Layout.fillWidth: true }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Objects Detected"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionUiAdapter.hasVisionTelemetry ? ("" + CompanionUiAdapter.detectionsCount) : "--"
                    font.bold: true
                    font.pointSize: ScreenTools.mediumFontPointSize
                }
            }

            Item { Layout.fillWidth: true }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Active Model"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionUiAdapter.hasVisionTelemetry && CompanionUiAdapter.modelName !== ""
                          ? CompanionUiAdapter.modelName
                          : "--"
                    font.bold: true
                    font.pointSize: ScreenTools.mediumFontPointSize
                }
            }
        }

        // Control: Input Source
        LabelledComboBox {
            Layout.fillWidth: true
            label: qsTr("AI Input Source")
            enabled: CompanionUiAdapter.hasVisionTelemetry
            model: CompanionUiAdapter.hasVisionTelemetry ? ["realsense", "siyi_gimbal"] : ["--"]
            currentIndex: CompanionUiAdapter.hasVisionTelemetry ? (root._draftSource === "siyi_gimbal" ? 1 : 0) : 0
            onActivated: (index) => {
                if (CompanionUiAdapter.hasVisionTelemetry) {
                    root._draftSource = (index === 1) ? "siyi_gimbal" : "realsense"
                }
            }
        }

        // Control: Detection Model
        LabelledComboBox {
            Layout.fillWidth: true
            label: qsTr("Detection Model")
            enabled: CompanionUiAdapter.hasVisionTelemetry
            model: CompanionUiAdapter.hasVisionTelemetry ? ["yolo26n.pt", "yolo26s.pt", "aruco_tracker.pt"] : ["--"]
            currentIndex: CompanionUiAdapter.hasVisionTelemetry
                          ? (root._draftModel === "yolo26s.pt" ? 1 : (root._draftModel === "aruco_tracker.pt" ? 2 : 0))
                          : 0
            onActivated: (index) => {
                if (CompanionUiAdapter.hasVisionTelemetry) {
                    if (index === 0) root._draftModel = "yolo26n.pt"
                    else if (index === 1) root._draftModel = "yolo26s.pt"
                    else root._draftModel = "aruco_tracker.pt"
                }
            }
        }

        // Control: Confidence Threshold Slider
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth * 1.5

            QGCLabel {
                Layout.fillWidth: true
                text: CompanionUiAdapter.hasVisionTelemetry
                      ? qsTr("Confidence Threshold (%1%)").arg(Math.round(root._draftConf * 100))
                      : qsTr("Confidence Threshold: --")
            }

            QGCSlider {
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 25
                enabled: CompanionUiAdapter.hasVisionTelemetry
                from: 0.10
                to: 0.95
                stepSize: 0.05
                value: CompanionUiAdapter.hasVisionTelemetry ? root._draftConf : 0.10
                onMoved: {
                    if (CompanionUiAdapter.hasVisionTelemetry) {
                        root._draftConf = value
                    }
                }
            }
        }

        // Action Buttons Row
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCButton {
                text: qsTr("Vision config unavailable")
                primary: true
                enabled: false
            }

            QGCButton {
                text: qsTr("Undo")
                enabled: CompanionUiAdapter.hasVisionTelemetry && root.hasPendingChanges
                onClicked: root.undoChanges()
            }

            Item { Layout.fillWidth: true }

            QGCButton {
                text: qsTr("Save Defaults")
                enabled: false
            }

            QGCButton {
                text: qsTr("Restore Defaults")
                enabled: false
            }
        }
    }

    // —— 2. Vision Activity Log ————————————————————————————————————————
    SettingsGroupLayout {
        Layout.fillWidth: true
        heading: qsTr("Vision Activity Log")

        CompanionLogPanel {
            Layout.fillWidth: true
            Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 12
            filterPrefix: "VIS:"
            serviceName: "Vision / YOLO"
        }
    }
}
