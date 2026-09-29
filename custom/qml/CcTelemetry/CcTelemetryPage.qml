import QGC as QGCNative
import QtQuick
import QGroundControl

Rectangle {
    id: root

    objectName: "settingsPage_CcTelemetry"
    color: qgcPal.window

    QGCPalette {
        id: qgcPal
    }

    CcTelemetryAdapter {
        id: telemetryAdapter

        previewMode: false
        source: QGCNative.CcTelemetryController
    }

    CcTelemetryPanel {
        anchors.fill: parent
        previewMode: false
        telemetry: telemetryAdapter
    }
}
