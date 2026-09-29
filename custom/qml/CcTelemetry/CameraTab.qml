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
            messageName: "CC_TELEMETRY_CAMERA"
            received: root.telemetry.cameraReceived
            stale: root.telemetry.cameraStale
        }

        GridLayout {
            Layout.fillWidth: true
            columnSpacing: 16
            columns: root.availableWidth >= 1050 ? 3 : (root.availableWidth >= 680 ? 2 : 1)
            rowSpacing: 16
            visible: root.telemetry.cameraReceived

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 198
                iconText: "CAM"
                subtitle: "CC_TELEMETRY_CAMERA"
                title: qsTr("Camera")

                KeyValueRow {
                    label: qsTr("Camera")
                    value: root.telemetry.camera.camera_type
                }

                KeyValueRow {
                    label: qsTr("Serial")
                    monospace: true
                    value: root.telemetry.camera.serial_number
                }
            }

            MetricCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 198
                accentColor: "#48c8ff"
                detail: "video_width / video_height / video_fps"
                iconText: "RGB"
                title: qsTr("RGB Stream")
                unit: root.telemetry.camera.video_fps + " fps"
                value: root.telemetry.camera.video_width + " x " + root.telemetry.camera.video_height
            }

            MetricCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 198
                accentColor: "#a58cff"
                detail: "depth_width / depth_height / depth_fps"
                iconText: "DEP"
                title: qsTr("Depth Stream")
                unit: root.telemetry.camera.depth_fps + " fps"
                value: root.telemetry.camera.depth_width + " x " + root.telemetry.camera.depth_height
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 246
                iconText: "CFG"
                subtitle: qsTr("Capture configuration")
                title: qsTr("Stream Settings")

                KeyValueRow {
                    label: qsTr("Rotation")
                    value: root.telemetry.camera.rotation + "°"
                }

                KeyValueRow {
                    label: qsTr("Profile Mode")
                    value: root.telemetry.profileMode(root.telemetry.camera.profile_mode)
                }

                KeyValueRow {
                    label: qsTr("Emitter")
                    value: root.telemetry.enabledText(root.telemetry.camera.enable_emitter)
                    valueColor: root.telemetry.camera.enable_emitter === 1 ? "#54dfa2" : "#8494a2"
                }
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 246
                contentSpacing: 9
                iconText: "ENC"
                subtitle: qsTr("Video compression")
                title: qsTr("Encoder")

                KeyValueRow {
                    label: qsTr("Codec")
                    value: root.telemetry.camera.codec.toUpperCase()
                }

                KeyValueRow {
                    label: qsTr("Mode")
                    value: root.telemetry.camera.encoder_mode
                }

                KeyValueRow {
                    label: qsTr("Bitrate")
                    value: root.telemetry.formatInteger(root.telemetry.camera.bitrate_kbps) + " kbps"
                }

                KeyValueRow {
                    label: qsTr("Max Bitrate")
                    value: root.telemetry.formatInteger(root.telemetry.camera.bitrate_max_kbps) + " kbps"
                }

                KeyValueRow {
                    label: qsTr("VBV Buffer")
                    value: root.telemetry.formatInteger(root.telemetry.camera.vbv_buffer_kb) + " kb"
                }
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 246
                iconText: "NET"
                subtitle: root.telemetry.previewMode ? qsTr("Preview-only state derived from URL") : qsTr("The MAVLink message has no stream-status field")
                title: qsTr("RTSP")

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        color: "#71899b"
                        font.pixelSize: 12
                        text: qsTr("Stream")
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    StatusBadge {
                        active: root.telemetry.rtspIsLive()
                        text: root.telemetry.previewMode ? (root.telemetry.rtspIsLive() ? qsTr("LIVE") : qsTr("NO URL")) : qsTr("N/A")
                    }
                }

                Label {
                    Layout.fillWidth: true
                    color: "#71899b"
                    font.pixelSize: 11
                    text: qsTr("URL")
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    TextField {
                        id: rtspUrl

                        Layout.fillWidth: true
                        color: "#cbd8e2"
                        font.family: "Consolas"
                        font.pixelSize: 11
                        readOnly: true
                        selectByMouse: true
                        text: root.telemetry.camera.rtsp_url

                        background: Rectangle {
                            border.color: "#1c3545"
                            color: "#09141d"
                            radius: 8
                        }
                    }

                    Button {
                        enabled: root.telemetry.camera.rtsp_url.length > 0
                        flat: true
                        palette.buttonText: "#48c8ff"
                        text: qsTr("COPY")

                        onClicked: {
                            rtspUrl.selectAll();
                            rtspUrl.copy();
                            rtspUrl.deselect();
                        }
                    }
                }
            }
        }
    }
}
