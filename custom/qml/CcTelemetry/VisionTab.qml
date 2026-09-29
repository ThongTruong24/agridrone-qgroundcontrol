import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root

    required property var telemetry

    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    clip: true
    contentWidth: availableWidth

    ColumnLayout {
        spacing: 16
        width: root.availableWidth

        DataStateBanner {
            messageName: "CC_TELEMETRY_VISION"
            received: root.telemetry.visionReceived
            stale: root.telemetry.visionStale
        }

        GridLayout {
            Layout.fillWidth: true
            columnSpacing: 16
            columns: root.availableWidth >= 1050 ? 3 : (root.availableWidth >= 680 ? 2 : 1)
            rowSpacing: 16
            visible: root.telemetry.visionReceived

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 198
                iconText: "AI"
                subtitle: "CC_TELEMETRY_VISION"
                title: qsTr("Pipeline")

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        color: "#71899b"
                        font.pixelSize: 12
                        text: qsTr("Vision pipeline")
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    StatusBadge {
                        active: root.telemetry.fresh(root.telemetry.visionReceived, root.telemetry.visionStale) && root.telemetry.bitIsSet(root.telemetry.vision.status_flags, 0)
                        text: root.telemetry.statusText(root.telemetry.visionReceived, root.telemetry.visionStale, root.telemetry.bitIsSet(root.telemetry.vision.status_flags, 0), qsTr("RUNNING"), qsTr("STOPPED"))
                    }
                }
            }

            MetricCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 198
                accentColor: "#48c8ff"
                detail: "inference_fps"
                iconText: "FPS"
                title: qsTr("Inference FPS")
                unit: "fps"
                value: root.telemetry.vision.inference_fps.toFixed(1)
            }

            MetricCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 198
                accentColor: "#f5c84c"
                detail: "detections_count"
                iconText: "DET"
                title: qsTr("Detections")
                unit: qsTr("objects")
                value: root.telemetry.vision.detections_count.toString()
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 238
                iconText: "MOD"
                subtitle: qsTr("Detector configuration")
                title: qsTr("Model & Input")

                KeyValueRow {
                    label: qsTr("Model")
                    value: root.telemetry.vision.model_name
                }

                KeyValueRow {
                    label: qsTr("Source")
                    value: root.telemetry.vision.input_source
                }

                KeyValueRow {
                    label: qsTr("Input Size")
                    value: root.telemetry.vision.input_width + " x " + root.telemetry.vision.input_height
                }

                KeyValueRow {
                    label: qsTr("Camera FPS")
                    value: root.telemetry.vision.video_fps + " fps"
                }
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 238
                iconText: "RUN"
                subtitle: qsTr("Inference state")
                title: qsTr("Runtime")

                KeyValueRow {
                    label: qsTr("Confidence Threshold")
                    value: root.telemetry.vision.confidence_thresh.toFixed(2)
                }

                KeyValueRow {
                    label: qsTr("Depth Enabled")
                    value: root.telemetry.enabledText(root.telemetry.bitIsSet(root.telemetry.vision.status_flags, 1))
                    valueColor: root.telemetry.bitIsSet(root.telemetry.vision.status_flags, 1) ? "#54dfa2" : "#8494a2"
                }

                KeyValueRow {
                    label: qsTr("Status Flags")
                    monospace: true
                    value: root.telemetry.hexByte(root.telemetry.vision.status_flags)
                }
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 238
                iconText: "BIT"
                subtitle: qsTr("Vision pipeline bitmask")
                title: qsTr("Status Flags")

                BitFlagRow {
                    bit: 0
                    flagSet: root.telemetry.bitIsSet(root.telemetry.vision.status_flags, 0)
                    label: qsTr("Running")
                }

                BitFlagRow {
                    bit: 1
                    flagSet: root.telemetry.bitIsSet(root.telemetry.vision.status_flags, 1)
                    label: qsTr("Depth enabled")
                }
            }
        }
    }
}
