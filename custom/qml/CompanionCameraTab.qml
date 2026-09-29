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

    property int    _activeCamId:       0
    property int    _draftCamId:        0

    property string _activeResolution:  ""
    property string _draftResolution:   ""
    property int    _activeFps:         0
    property int    _draftFps:          0
    property string _activeCodec:       ""
    property string _draftCodec:        ""
    property int    _activeBitrate:     0
    property int    _draftBitrate:      0

    readonly property string _rtspQgc:  (CompanionController.hasCameraTelemetry && CompanionController.rtspUrlQgc !== "") ? CompanionController.rtspUrlQgc : "--"
    readonly property string _rtspSiyi: (CompanionController.hasCameraTelemetry && CompanionController.rtspUrlController !== "") ? CompanionController.rtspUrlController : "--"

    readonly property bool hasPendingChanges: (
        CompanionController.hasCameraTelemetry &&
        (_draftCamId !== _activeCamId ||
         (_draftResolution !== "" && _draftResolution !== _activeResolution) ||
         (_draftFps > 0 && _draftFps !== _activeFps) ||
         (_draftBitrate > 0 && _draftBitrate !== _activeBitrate))
    )

    function applyAllChanges() {
        if (!CompanionController.hasCameraTelemetry) return
        var parts = _draftResolution.split("x")
        var w = parseInt(parts[0]) || 1280
        var h = parseInt(parts[1]) || 720
        CompanionController.sendCameraConfig(w, h, _draftFps, _draftBitrate, 0, _draftCodec)
        CompanionController.applyConfig(2, true)
        _activeCamId = _draftCamId
        _activeResolution = _draftResolution
        _activeFps = _draftFps
        _activeBitrate = _draftBitrate
    }

    function saveAsDefault() {
        if (!CompanionController.hasCameraTelemetry) return
        var parts = _draftResolution.split("x")
        var w = parseInt(parts[0]) || 1280
        var h = parseInt(parts[1]) || 720
        CompanionController.sendCameraConfig(w, h, _draftFps, _draftBitrate, 0, _draftCodec)
        CompanionController.saveDefaultConfig(2)
    }

    function restoreDefaults() {
        if (!CompanionController.hasCameraTelemetry) return
        CompanionController.restoreDefaultConfig(2)
    }

    function assignToQgcVideo() {
        if (_rtspQgc !== "--") {
            var vs = QGroundControl.settingsManager.videoSettings
            vs.videoSource.rawValue = vs.rtspVideoSource
            vs.rtspUrl.rawValue = _rtspQgc
            QGroundControl.videoManager.restartVideo()
        }
    }

    Connections {
        target: CompanionController
        function onCameraChanged() {
            if (CompanionController.hasCameraTelemetry) {
                root._activeResolution = "" + CompanionController.videoWidth + "x" + CompanionController.videoHeight
                root._activeFps = CompanionController.videoFps
                root._activeBitrate = CompanionController.bitrateKbps
                root._draftResolution = root._activeResolution
                root._draftFps = root._activeFps
                root._draftBitrate = root._activeBitrate
            } else {
                root._activeResolution = ""
                root._activeFps = 0
                root._activeBitrate = 0
                root._draftResolution = ""
                root._draftFps = 0
                root._draftBitrate = 0
            }
        }
    }

    // —— 1. Camera Hardware & Configuration —————————————————————————————
    SettingsGroupLayout {
        Layout.fillWidth: true
        heading: qsTr("Camera Hardware & Stream Configuration")

        // Status Row
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth * 2

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Camera Status"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionController.hasCameraTelemetry ? qsTr("ONLINE") : "--"
                    font.bold: true
                    font.pointSize: ScreenTools.mediumFontPointSize
                    color: CompanionController.hasCameraTelemetry ? "#2ECC71" : qgcPal.text
                }
            }

            Item { Layout.fillWidth: true }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Active Stream"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionController.hasCameraTelemetry && _activeResolution !== ""
                          ? (_activeResolution + " @ " + _activeFps + " FPS")
                          : "--"
                    font.bold: true
                    font.pointSize: ScreenTools.mediumFontPointSize
                }
            }

            Item { Layout.fillWidth: true }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("USB Bus Mode"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionController.hasCameraTelemetry
                          ? (CompanionController.usbSpeedMode === 2 ? "USB 3.0 (SuperSpeed)" : "USB 2.0 (HighSpeed)")
                          : "--"
                    font.bold: true
                    font.pointSize: ScreenTools.mediumFontPointSize
                }
            }
        }

        // Camera Model Selector
        LabelledComboBox {
            Layout.fillWidth: true
            label: qsTr("Camera Model")
            enabled: CompanionController.hasCameraTelemetry
            model: CompanionController.hasCameraTelemetry
                   ? [qsTr("Camera 1: Intel RealSense D435i"), qsTr("Camera 2: SIYI Gimbal A8 mini")]
                   : ["--"]
            currentIndex: root._draftCamId
            onActivated: (index) => { if (CompanionController.hasCameraTelemetry) root._draftCamId = index }
        }

        // Resolution Selector
        LabelledComboBox {
            Layout.fillWidth: true
            label: qsTr("Resolution")
            enabled: CompanionController.hasCameraTelemetry
            model: CompanionController.hasCameraTelemetry
                   ? ["1280x720", "1920x1080", "848x480", "640x480"]
                   : ["--"]
            currentIndex: CompanionController.hasCameraTelemetry ? Math.max(0, model.indexOf(root._draftResolution)) : 0
            onActivated: (index) => { if (CompanionController.hasCameraTelemetry) root._draftResolution = model[index] }
        }

        // Framerate Selector
        LabelledComboBox {
            Layout.fillWidth: true
            label: qsTr("Frame Rate")
            enabled: CompanionController.hasCameraTelemetry
            model: CompanionController.hasCameraTelemetry ? ["30 FPS", "60 FPS", "15 FPS"] : ["--"]
            currentIndex: CompanionController.hasCameraTelemetry ? (root._draftFps === 60 ? 1 : (root._draftFps === 15 ? 2 : 0)) : 0
            onActivated: (index) => {
                if (CompanionController.hasCameraTelemetry) {
                    root._draftFps = (index === 1) ? 60 : ((index === 2) ? 15 : 30)
                }
            }
        }

        // Bitrate Selector
        LabelledComboBox {
            Layout.fillWidth: true
            label: qsTr("Bitrate")
            enabled: CompanionController.hasCameraTelemetry
            model: CompanionController.hasCameraTelemetry ? ["2000 kbps", "4000 kbps", "1000 kbps", "8000 kbps"] : ["--"]
            currentIndex: CompanionController.hasCameraTelemetry
                          ? (root._draftBitrate === 4000 ? 1 : (root._draftBitrate === 1000 ? 2 : (root._draftBitrate === 8000 ? 3 : 0)))
                          : 0
            onActivated: (index) => {
                if (CompanionController.hasCameraTelemetry) {
                    root._draftBitrate = parseInt(model[index]) || 2000
                }
            }
        }

        // Actions
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCButton {
                text: qsTr("Apply Camera Config")
                primary: true
                enabled: CompanionController.hasCameraTelemetry && root.hasPendingChanges
                onClicked: root.applyAllChanges()
            }

            Item { Layout.fillWidth: true }

            QGCButton {
                text: qsTr("Save Defaults")
                enabled: CompanionController.hasCameraTelemetry
                onClicked: root.saveAsDefault()
            }

            QGCButton {
                text: qsTr("Restore Defaults")
                enabled: CompanionController.hasCameraTelemetry
                onClicked: root.restoreDefaults()
            }
        }
    }

    // —— 2. RTSP Video Stream Endpoints ————————————————————————————————
    SettingsGroupLayout {
        Layout.fillWidth: true
        heading: qsTr("RTSP Video Stream Endpoints")

        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCLabel {
                text: qsTr("QGC Stream:")
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 14
            }

            QGCTextField {
                Layout.fillWidth: true
                readOnly: true
                text: root._rtspQgc
            }

            QGCButton {
                text: qsTr("Assign to QGC")
                enabled: CompanionController.hasCameraTelemetry && root._rtspQgc !== "--"
                onClicked: root.assignToQgcVideo()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCLabel {
                text: qsTr("Remote Stream:")
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 14
            }

            QGCTextField {
                Layout.fillWidth: true
                readOnly: true
                text: root._rtspSiyi
            }
        }
    }

    // —— 3. Camera Stream Activity Log —————————————————————————————————
    SettingsGroupLayout {
        Layout.fillWidth: true
        heading: qsTr("Camera Stream Activity Log")

        CompanionLogPanel {
            Layout.fillWidth: true
            Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 12
            filterPrefix: "CAM:"
            serviceName: "Camera RTSP"
        }
    }
}
