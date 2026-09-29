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
            messageName: "CC_TELEMETRY_NETWORK"
            received: root.telemetry.networkReceived
            stale: root.telemetry.networkStale
        }

        GridLayout {
            Layout.fillWidth: true
            columnSpacing: 16
            columns: root.availableWidth >= 1050 ? 3 : (root.availableWidth >= 680 ? 2 : 1)
            rowSpacing: 16
            visible: root.telemetry.networkReceived

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 198
                iconText: "AP"
                subtitle: root.telemetry.previewMode ? qsTr("Preview-only state derived from SSID + dnsmasq") : qsTr("The MAVLink message has no AP-online field")
                title: qsTr("Access Point")

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        color: "#f0f5f8"
                        font.pixelSize: 20
                        font.weight: Font.Bold
                        text: root.telemetry.network.ap_ssid
                    }

                    StatusBadge {
                        active: root.telemetry.accessPointIsOnline()
                        text: root.telemetry.previewMode ? (root.telemetry.accessPointIsOnline() ? qsTr("ONLINE") : qsTr("OFFLINE")) : qsTr("N/A")
                    }
                }
            }

            MetricCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 198
                accentColor: "#48c8ff"
                detail: "ap_client_count"
                iconText: "CLI"
                title: qsTr("Clients")
                unit: qsTr("connected")
                value: root.telemetry.network.ap_client_count.toString()
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 198
                iconText: "DHCP"
                subtitle: qsTr("Access point address service")
                title: qsTr("DHCP")

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        color: "#71899b"
                        font.pixelSize: 12
                        text: qsTr("dnsmasq")
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    StatusBadge {
                        active: root.telemetry.fresh(root.telemetry.networkReceived, root.telemetry.networkStale) && root.telemetry.network.dnsmasq_status === 1
                        text: root.telemetry.statusText(root.telemetry.networkReceived, root.telemetry.networkStale, root.telemetry.network.dnsmasq_status === 1, qsTr("ACTIVE"), qsTr("INACTIVE"))
                    }
                }
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 254
                iconText: "IP"
                subtitle: qsTr("Network interfaces")
                title: qsTr("IP Addresses")

                KeyValueRow {
                    label: "eth0"
                    monospace: true
                    value: root.telemetry.network.eth0_ip
                }

                KeyValueRow {
                    label: "wlan0"
                    monospace: true
                    value: root.telemetry.network.wlan0_ip
                }

                KeyValueRow {
                    label: "uap0"
                    monospace: true
                    value: root.telemetry.network.ap_ip
                }
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 254
                contentSpacing: 7
                iconText: "CFG"
                subtitle: qsTr("hostapd configuration")
                title: qsTr("Access Point Settings")

                KeyValueRow {
                    label: qsTr("Channel")
                    value: root.telemetry.network.ap_channel.toString()
                }

                KeyValueRow {
                    label: qsTr("Mode")
                    value: root.telemetry.network.ap_hw_mode
                }

                KeyValueRow {
                    label: "802.11n"
                    value: root.telemetry.enabledText(root.telemetry.network.ap_ieee80211n)
                    valueColor: root.telemetry.network.ap_ieee80211n === 1 ? "#54dfa2" : "#8494a2"
                }

                KeyValueRow {
                    label: "WMM"
                    value: root.telemetry.enabledText(root.telemetry.network.ap_wmm_enabled)
                    valueColor: root.telemetry.network.ap_wmm_enabled === 1 ? "#54dfa2" : "#8494a2"
                }

                KeyValueRow {
                    label: "WPA"
                    value: root.telemetry.network.ap_wpa === 2 ? "WPA2" : root.telemetry.network.ap_wpa.toString()
                }

                KeyValueRow {
                    label: qsTr("Key Mgmt")
                    value: root.telemetry.network.ap_key_mgmt
                }
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 254
                iconText: "SEC"
                subtitle: qsTr("Credentials and services")
                title: qsTr("Security")

                KeyValueRow {
                    label: qsTr("Passphrase")
                    monospace: true
                    value: root.telemetry.maskedSecret(root.telemetry.network.ap_wpa_passphrase)
                }

                KeyValueRow {
                    label: qsTr("dnsmasq status")
                    value: root.telemetry.activeText(root.telemetry.network.dnsmasq_status)
                    valueColor: root.telemetry.network.dnsmasq_status === 1 ? "#54dfa2" : "#8494a2"
                }

                KeyValueRow {
                    label: qsTr("Wi-Fi Client DHCP")
                    value: root.telemetry.activeText(root.telemetry.network.wlan0_dhcp)
                    valueColor: root.telemetry.network.wlan0_dhcp === 1 ? "#54dfa2" : "#8494a2"
                }
            }
        }
    }
}
