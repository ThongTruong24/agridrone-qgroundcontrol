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
            messageName: "CC_TELEMETRY_LINKS"
            received: root.telemetry.linksReceived
            stale: root.telemetry.linksStale
        }

        GridLayout {
            Layout.fillWidth: true
            columnSpacing: 16
            columns: root.availableWidth >= 1050 ? 3 : (root.availableWidth >= 680 ? 2 : 1)
            rowSpacing: 16
            visible: root.telemetry.linksReceived

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 214
                iconText: "FC"
                subtitle: "CC_TELEMETRY_LINKS"
                title: qsTr("FC Link")

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        color: "#71899b"
                        font.pixelSize: 12
                        text: qsTr("Connection")
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    StatusBadge {
                        active: root.telemetry.fresh(root.telemetry.linksReceived, root.telemetry.linksStale) && root.telemetry.bitIsSet(root.telemetry.links.link_status_flags, 0)
                        text: root.telemetry.statusText(root.telemetry.linksReceived, root.telemetry.linksStale, root.telemetry.bitIsSet(root.telemetry.links.link_status_flags, 0), qsTr("CONNECTED"), qsTr("OFFLINE"))
                    }
                }

                KeyValueRow {
                    label: qsTr("Port")
                    monospace: true
                    value: root.telemetry.links.fc_port
                }

                KeyValueRow {
                    label: qsTr("Baudrate")
                    value: root.telemetry.formatBaudrate(root.telemetry.links.fc_baudrate)
                }
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 214
                iconText: "SY"
                subtitle: "CC_TELEMETRY_LINKS"
                title: qsTr("SIYI Link")

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        color: "#71899b"
                        font.pixelSize: 12
                        text: qsTr("Connection")
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    StatusBadge {
                        active: root.telemetry.fresh(root.telemetry.linksReceived, root.telemetry.linksStale) && root.telemetry.bitIsSet(root.telemetry.links.link_status_flags, 1)
                        text: root.telemetry.statusText(root.telemetry.linksReceived, root.telemetry.linksStale, root.telemetry.bitIsSet(root.telemetry.links.link_status_flags, 1), qsTr("CONNECTED"), qsTr("OFFLINE"))
                    }
                }

                KeyValueRow {
                    label: qsTr("Port")
                    monospace: true
                    value: root.telemetry.links.siyi_port
                }

                KeyValueRow {
                    label: qsTr("Baudrate")
                    value: root.telemetry.formatBaudrate(root.telemetry.links.siyi_baudrate)
                }
            }

            MetricCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 214
                accentColor: "#f5c84c"
                detail: "fc_bitrate_kbps"
                iconText: "KB"
                title: qsTr("Live Bitrate")
                unit: "kbps"
                value: root.telemetry.links.fc_bitrate_kbps.toFixed(1)
            }

            MetricCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 214
                accentColor: "#48c8ff"
                detail: qsTr("Total received from flight controller")
                iconText: "RX"
                title: qsTr("FC Bytes RX")
                value: root.telemetry.formatInteger(root.telemetry.links.fc_bytes_rx)
            }

            MetricCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 214
                accentColor: "#a58cff"
                detail: qsTr("Total sent to flight controller")
                iconText: "TX"
                title: qsTr("FC Bytes TX")
                value: root.telemetry.formatInteger(root.telemetry.links.fc_bytes_tx)
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 214
                iconText: "BIT"
                subtitle: qsTr("Connection bitmask")
                title: qsTr("Link Flags")

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        color: "#71899b"
                        font.pixelSize: 12
                        text: qsTr("Raw value")
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    StatusBadge {
                        active: true
                        activeColor: "#f5c84c"
                        showDot: false
                        text: root.telemetry.hexByte(root.telemetry.links.link_status_flags)
                    }
                }

                BitFlagRow {
                    bit: 0
                    flagSet: root.telemetry.bitIsSet(root.telemetry.links.link_status_flags, 0)
                    label: qsTr("FC connected")
                }

                BitFlagRow {
                    bit: 1
                    flagSet: root.telemetry.bitIsSet(root.telemetry.links.link_status_flags, 1)
                    label: qsTr("SIYI connected")
                }
            }
        }
    }
}
