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
        CompanionController.hasVisionTelemetry &&
        (Math.abs(_draftConf - _activeConf) > 0.01 ||
         _draftModel !== _activeModel ||
         _draftSource !== _activeSource)
    )

    function applyAllChanges() {
        if (!CompanionController.hasVisionTelemetry) return
        CompanionController.sendVisionConfig(_draftConf, _draftModel, _draftSource)
        CompanionController.applyConfig(4, true)
        _activeConf = _draftConf
        _activeModel = _draftModel
        _activeSource = _draftSource
    }

    function saveAsDefault() {
        if (!CompanionController.hasVisionTelemetry) return
        CompanionController.sendVisionConfig(_draftConf, _draftModel, _draftSource)
        CompanionController.saveDefaultConfig(4)
    }

    function restoreDefaults() {
        if (!CompanionController.hasVisionTelemetry) return
        CompanionController.restoreDefaultConfig(4)
    }

    function undoChanges() {
        _draftConf = _activeConf
        _draftModel = _activeModel
        _draftSource = _activeSource
    }

    Connections {
        target: CompanionController
        function onVisionChanged() {
            if (CompanionController.hasVisionTelemetry) {
                root._activeConf = CompanionController.confidenceThresh
                root._draftConf = CompanionController.confidenceThresh
                root._activeModel = CompanionController.modelName !== "" ? CompanionController.modelName : "--"
                root._draftModel = root._activeModel
                root._activeSource = CompanionController.inputSource !== "" ? CompanionController.inputSource : "--"
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
                    text: CompanionController.hasVisionTelemetry && CompanionController.inferenceFps > 0
                          ? (CompanionController.inferenceFps.toFixed(1) + " FPS")
                          : "--"
                    font.bold: true
                    font.pointSize: ScreenTools.mediumFontPointSize
                    color: CompanionController.hasVisionTelemetry ? qgcPal.colorGreen : qgcPal.text
                }
            }

            Item { Layout.fillWidth: true }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Objects Detected"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionController.hasVisionTelemetry ? ("" + CompanionController.detectionsCount) : "--"
                    font.bold: true
                    font.pointSize: ScreenTools.mediumFontPointSize
                }
            }

            Item { Layout.fillWidth: true }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Active Model"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionController.hasVisionTelemetry && CompanionController.modelName !== ""
                          ? CompanionController.modelName
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
            enabled: CompanionController.hasVisionTelemetry
            model: CompanionController.hasVisionTelemetry ? ["realsense", "siyi_gimbal"] : ["--"]
            currentIndex: CompanionController.hasVisionTelemetry ? (root._draftSource === "siyi_gimbal" ? 1 : 0) : 0
            onActivated: (index) => {
                if (CompanionController.hasVisionTelemetry) {
                    root._draftSource = (index === 1) ? "siyi_gimbal" : "realsense"
                }
            }
        }

        // Control: Detection Model
        LabelledComboBox {
            Layout.fillWidth: true
            label: qsTr("Detection Model")
            enabled: CompanionController.hasVisionTelemetry
            model: CompanionController.hasVisionTelemetry ? ["yolo26n.pt", "yolo26s.pt", "aruco_tracker.pt"] : ["--"]
            currentIndex: CompanionController.hasVisionTelemetry
                          ? (root._draftModel === "yolo26s.pt" ? 1 : (root._draftModel === "aruco_tracker.pt" ? 2 : 0))
                          : 0
            onActivated: (index) => {
                if (CompanionController.hasVisionTelemetry) {
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
                text: CompanionController.hasVisionTelemetry
                      ? qsTr("Confidence Threshold (%1%)").arg(Math.round(root._draftConf * 100))
                      : qsTr("Confidence Threshold: --")
            }

            QGCSlider {
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 25
                enabled: CompanionController.hasVisionTelemetry
                from: 0.10
                to: 0.95
                stepSize: 0.05
                value: CompanionController.hasVisionTelemetry ? root._draftConf : 0.10
                onMoved: {
                    if (CompanionController.hasVisionTelemetry) {
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
                text: qsTr("Apply Vision Config")
                primary: true
                enabled: CompanionController.hasVisionTelemetry && root.hasPendingChanges
                onClicked: root.applyAllChanges()
            }

            QGCButton {
                text: qsTr("Undo")
                enabled: CompanionController.hasVisionTelemetry && root.hasPendingChanges
                onClicked: root.undoChanges()
            }

            Item { Layout.fillWidth: true }

            QGCButton {
                text: qsTr("Save Defaults")
                enabled: CompanionController.hasVisionTelemetry
                onClicked: root.saveAsDefault()
            }

            QGCButton {
                text: qsTr("Restore Defaults")
                enabled: CompanionController.hasVisionTelemetry
                onClicked: root.restoreDefaults()
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
