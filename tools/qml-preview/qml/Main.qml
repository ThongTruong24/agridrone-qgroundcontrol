import CC.TelemetryPreview
import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: root

    color: "#071019"
    height: 900
    minimumHeight: 620
    minimumWidth: 820
    title: qsTr("CC Telemetry - Qt Quick Preview")
    visible: true
    width: 1440

    onClosing: Qt.quit()

    MockCcTelemetry {
        id: mockSource
    }

    CcTelemetryAdapter {
        id: telemetryAdapter

        previewMode: true
        source: mockSource
    }

    Shortcut {
        sequence: "Ctrl+R"

        // qmllint disable unqualified
        onActivated: previewController.reload()
        // qmllint enable unqualified
    }

    Rectangle {
        anchors.fill: parent

        gradient: Gradient {
            GradientStop {
                color: "#081521"
                position: 0.0
            }

            GradientStop {
                color: "#071019"
                position: 0.55
            }

            GradientStop {
                color: "#050b12"
                position: 1.0
            }
        }

        Loader {
            id: panelLoader

            anchors.fill: parent

            // qmllint disable unqualified
            Component.onCompleted: setSource(previewSharedPanelUrl, {
                "animateData": mockSource.animate,
                "previewMode": true,
                "telemetry": telemetryAdapter
            })
            // qmllint enable unqualified
        }

        Connections {
            function onAnimateDataChanged() {
                mockSource.animate = panelLoader.item.animateData;
            }

            function onResetRequested() {
                mockSource.reset();
            }

            target: panelLoader.item
        }
    }
}
